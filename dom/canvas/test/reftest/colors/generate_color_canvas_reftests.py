#! python3

# Typecheck:
# `pip -m mypy generate_color_canvas_reftests.py`
#
# Run:
# `./generate_color_canvas_reftests.py [--write]`

import functools
import json
import math
import pathlib
import re
import sys
from typing import Iterable, NamedTuple, TypeVar

ARGS = sys.argv[1:]
DEST = pathlib.Path(__file__).parent / "_generated_reftest.list"

COL_DELIM = "  "
COL_ALIGNMENT = 4

# -

T = TypeVar("T")
U = TypeVar("U")

# -


# crossCombine([{a:false},{a:5}], [{},{b:5}])
# [{a:false}, {a:true}, {a:false,b:5}, {a:true,b:5}]
def cross_combine(*args_tup: list[dict]) -> list[dict]:
    args = list(args_tup)
    for i, a in enumerate(args):
        assert type(a) == list, f"Arg{i} is {type(a)}, expected {list}."

    def cross_combine2(listA, listB):
        listC = []
        for a in listA:
            for b in listB:
                c = dict()
                c.update(a)
                c.update(b)
                listC.append(c)
        return listC

    res: list[dict] = [dict()]
    while True:
        try:
            next = args.pop(0)
        except IndexError:
            break
        res = cross_combine2(res, next)
    return res


# keyed_alternatives('count', [1,2,3]) -> [{count: 1}, {count: 2}, {count: 3}]
def keyed_alternatives(key: T, vals: Iterable[U]) -> list[dict[T, U]]:
    """
    res = []
    for v in vals:
        d = dict()
        d[key] = v
        res.append(d)
    return res
    """
    # return [dict([[key,v]]) for v in vals]
    return [{key: v} for v in vals]


# -


def eprint(*args, **kwargs):
    sys.stdout.flush()
    print(*args, file=sys.stderr, **kwargs)
    sys.stderr.flush()


# -

# color_canvas.html?e_width=100&e_height=100&e_context=css&e_options={}&e_cspace=&e_webgl_format=&e_color=rgb(127,0,0)

CSPACE_LIST = ["srgb", "display-p3"]
CANVAS_CSPACES = keyed_alternatives("e_cspace", CSPACE_LIST)

RGB_LIST = [
    "0.000 0.000 0.000",
    "0.200 0.200 0.200",  # 0.2*255 = 51
    "0.200 0.000 0.000",
    "0.000 0.200 0.000",
    "0.000 0.000 0.200",
    "0.502 0.502 0.502",  # 0.502*255 = 128.01
    "0.502 0.000 0.000",
    "0.000 0.502 0.000",
    "0.000 0.000 0.502",
    "1.000 1.000 1.000",
    #'1.000 0.000 0.000', # These will hit gamut clipping on most displays.
    #'0.000 1.000 0.000',
    #'0.000 0.000 1.000',
]

WEBGL_COLORS = keyed_alternatives("e_color", [f"color(srgb {rgb})" for rgb in RGB_LIST])

C2D_COLORS = []
for cspace in CSPACE_LIST:
    C2D_COLORS += keyed_alternatives(
        "e_color", [f"color({cspace} {rgb})" for rgb in RGB_LIST]
    )

# -

WEBGL_FORMATS = keyed_alternatives(
    "e_webgl_format",
    [
        "RGBA8",
        # Bug 1883748: (webgl.drawingbufferStorage)
        #'SRGB8_ALPHA8',
        #'RGBA16F',
    ],
)
WEBGL_VERSIONS = keyed_alternatives("e_context", ["webgl", "webgl2"])
WEBGL = cross_combine(WEBGL_VERSIONS, WEBGL_FORMATS, CANVAS_CSPACES, WEBGL_COLORS)

# -

C2D_OPTIONS_COMBOS = cross_combine(
    keyed_alternatives("willReadFrequently", ["true", "false"]),  # E.g. D2D vs Skia
    # keyed_alternatives('alpha', ['true','false'])
)
C2D_OPTIONS = [
    json.dumps(config, separators=(",", ":")) for config in C2D_OPTIONS_COMBOS
]

C2D = cross_combine(
    [{"e_context": "2d"}],
    keyed_alternatives("e_options", C2D_OPTIONS),
    CANVAS_CSPACES,
    C2D_COLORS,
)

# -

COMBOS: list[dict[str, str]] = cross_combine(WEBGL + C2D)

eprint(f"{len(COMBOS)} combinations...")

# -

Config = dict[str, str]


class CssColor(NamedTuple):
    cspace: str
    rgb: str

    def rgb_vals(self) -> tuple[float, float, float]:
        (r, g, b) = [float(z) for z in self.rgb.split(" ")]
        return (r, g, b)

    def is_same_color(x, y) -> bool:
        if x == y:
            return True
        (r, g, b) = x.rgb_vals()
        if x.rgb == y.rgb and r == g and g == b:
            return True
        return False


class Reftest(NamedTuple):
    notes: list[str]
    op: str
    test_config: Config
    ref_config: Config


def make_ref_config(color: CssColor) -> Config:
    return {
        "e_context": "css",
        "e_color": f"color({color.cspace} {color.rgb})",
    }


class ColorReftest(NamedTuple):
    notes: list[str]
    test_config: Config
    ref_color: CssColor

    def to_reftest(self):
        ref_config = make_ref_config(self.ref_color)
        return Reftest(self.notes.copy(), "==", self.test_config.copy(), ref_config)


class Expectation(NamedTuple):
    notes: list[str]
    test_config: Config
    color: CssColor


def parse_css_color(s: str) -> CssColor:
    m = re.match("color[(]([^)]+)[)]", s)
    assert m, s
    (cspace, rgb) = m.group(1).split(" ", 1)
    return CssColor(cspace, rgb)


def correct_color_from_test_config(test_config: Config) -> CssColor:
    canvas_cspace = test_config["e_cspace"]
    if not canvas_cspace:
        canvas_cspace = "srgb"

    correct_color = parse_css_color(test_config["e_color"])
    if test_config["e_context"].startswith("webgl"):
        # Webgl ignores the color's cspace, because webgl has no concept of
        # source colorspace for clears/draws to the backbuffer.
        # This (correct) behavior is as if the color's cspace were overwritten by the
        # cspace of the canvas. (so expect that)
        correct_color = CssColor(canvas_cspace, correct_color.rgb)

    return correct_color


# -------------------------------------
# -------------------------------------
# -------------------------------------
# Choose (multiple?) reference configs given a test config.


def reftests_from_config(test_config: Config) -> Iterable[ColorReftest]:
    correct_color = correct_color_from_test_config(test_config)

    if test_config["e_context"] in ["2d"]:
        # Canvas2d generally has the same behavior as css, so expect all passing.
        yield ColorReftest([], test_config, correct_color)
        return

    assert test_config["e_context"].startswith("webgl"), test_config["e_context"]

    # -

    def reftests_from_expected_color(
        notes: list[str], expected_color: CssColor
    ) -> Iterable[ColorReftest]:
        # If expecting failure, generate two tests, both expecting failure:
        # 1. expected-fail test == correct_color
        # 2. expected-pass test == (incorrect) expected_color
        # If we fix an error, we'll see one unexpected-pass and one unexpected-fail.
        # If we get a new wrong answer, we'll see one unexpected-fail.

        if correct_color.is_same_color(
            parse_css_color("color(display-p3 0.502 0.000 0.000)")
        ):
            notes += ["fuzzy(0-1,0-10000)"]

        if not expected_color.is_same_color(correct_color):
            yield ColorReftest(notes + ["fails"], test_config, correct_color)
            yield ColorReftest(notes, test_config, expected_color)
        else:
            yield ColorReftest(notes, test_config, correct_color)

    # -
    # On Mac, (with the pref) we do tag the IOSurface with the right cspace.
    # On other platforms, Webgl always outputs in the display color profile
    # right now. This is the same as "srgb".

    expected_color_srgb = CssColor("srgb", correct_color.rgb)
    if test_config["e_context"] == "webgl2":
        # Win, Mac
        yield from reftests_from_expected_color(
            ["skip-if(!cocoaWidget&&!winWidget)"], correct_color
        )
        # Lin, Android
        yield from reftests_from_expected_color(
            ["skip-if(cocoaWidget||winWidget)  "], expected_color_srgb
        )
    elif test_config["e_context"] == "webgl":
        # Mac
        yield from reftests_from_expected_color(
            ["skip-if(!cocoaWidget)"], correct_color
        )
        # Win, Lin, Android
        yield from reftests_from_expected_color(
            ["skip-if(cocoaWidget) "], expected_color_srgb
        )
    else:
        assert False, test_config["e_context"]


# -


def amended_notes_from_reftest(reftest: ColorReftest) -> list[str]:
    notes = reftest.notes[:]

    ref_rgb_vals = reftest.ref_color.rgb_vals()
    is_green_only = ref_rgb_vals == (0, ref_rgb_vals[1], 0)
    if (
        "fails" in reftest.notes
        and reftest.test_config["e_context"].startswith("webgl")
        and reftest.test_config["e_cspace"] == "display-p3"
        and is_green_only
    ):
        # Android's display bitdepth rounds srgb green and p3 green to the same thing.
        notes[notes.index("fails")] = "fails-if(!Android)"

    return notes


# -------------------------------------
# -------------------------------------
# -------------------------------------
# Ok, back to implementation.


def encode_url_v(k, v):
    if k == "e_color":
        # reftest harness can't deal with spaces in urls, and 'color(srgb%201%200%200)' is hard to read.
        v = v.replace(" ", ",")

    assert " " not in v, (k, v)
    return v


# Cool:
assert encode_url_v("e_color", "color(srgb 0 0 0)") == "color(srgb,0,0,0)"
# Unfortunate, but tolerable:
assert encode_url_v("e_color", "color(srgb   0   0   0)") == "color(srgb,,,0,,,0,,,0)"

# -


def url_from_config(kvs: Config) -> str:
    parts = [f"{k}={encode_url_v(k,v)}" for k, v in kvs.items()]
    url = "color_canvas.html?" + "&".join(parts)
    return url


# -

color_reftests: list[ColorReftest] = []
for c in COMBOS:
    color_reftests += reftests_from_config(c)
color_reftests = [
    ColorReftest(amended_notes_from_reftest(r), r.test_config, r.ref_color)
    for r in color_reftests
]
reftests = [r.to_reftest() for r in color_reftests]

# -

HEADINGS = ["# annotations", "op", "test", "reference"]
table: list[list[str]] = [HEADINGS]
table = [
    [
        " ".join(r.notes),
        r.op,
        url_from_config(r.test_config),
        url_from_config(r.ref_config),
    ]
    for r in reftests
]

# -


def round_to(a, b: int) -> int:
    return int(math.ceil(a / b) * b)


def aligned_lines_from_table(
    rows: list[list[str]], col_delim="  ", col_alignment=4
) -> Iterable[str]:
    max_col_len = functools.reduce(
        lambda accum, input: [max(r, len(c)) for r, c in zip(accum, input)],
        rows,
        [0 for _ in rows[0]],
    )
    max_col_len = [round_to(x, col_alignment) for x in max_col_len]

    for i, row in enumerate(rows):
        parts = [s + (" " * (col_len - len(s))) for s, col_len in zip(row, max_col_len)]
        line = col_delim.join(parts)
        yield line


# -

GENERATED_FILE_LINE = "### Generated, do not edit. ###"

lines = list(aligned_lines_from_table(table, COL_DELIM, COL_ALIGNMENT))
WARN_EVERY_N_LINES = 5
i = WARN_EVERY_N_LINES - 1
while i < len(lines):
    lines.insert(i, "  " + GENERATED_FILE_LINE)
    i += WARN_EVERY_N_LINES

# -

GENERATED_BY_ARGS = [f"./{pathlib.Path(__file__).name}"] + ARGS

REFTEST_LIST_PREAMBLE = f"""\
{GENERATED_FILE_LINE}
 {GENERATED_FILE_LINE}
  {GENERATED_FILE_LINE}

# Generated by `{' '.join(GENERATED_BY_ARGS)}`.
# -

defaults pref(webgl.colorspaces.prototype,true)

{GENERATED_FILE_LINE}
# -

# Ensure not white-screening:
!=  {url_from_config({})+'='}  about:blank
# Ensure differing results with different args:
!=  {url_from_config({'e_color':'color(srgb 1 0 0)'})}  {url_from_config({'e_color':'color(srgb 0 1 0)'})}

{GENERATED_FILE_LINE}
# -
"""

lines.insert(0, REFTEST_LIST_PREAMBLE)
lines.append("")

# -

for line in lines:
    print(line)

if "--write" not in ARGS:
    eprint("Use --write to write. Exiting...")
    sys.exit(0)

# -

eprint("Concatenating...")
file_str = "\n".join([line.rstrip() for line in lines])

eprint(f"Writing to {DEST}...")
DEST.write_bytes(file_str.encode())
eprint("Done!")

sys.exit(0)
