#! /usr/bin/env python3
import pathlib
import re
import sys

assert len(sys.argv) == 2
MOCHI_PATH = pathlib.Path(sys.argv[1])
assert MOCHI_PATH.suffix == ".html"

TEST_PATH = MOCHI_PATH.with_suffix(".solo.html")


def read_local_file(include):
    inc_path = MOCHI_PATH.parent
    file_path = inc_path / include

    try:
        return file_path.read_bytes()
    except IOError:
        return b""


SIMPLETEST_REPLACEMENT = b"""

<script>
// SimpleTest.js replacement

function debug(text) {
  var elem = document.getElementById('mochi-to-testcase-output');
  elem.innerHTML += '\\n<br/>\\n' + text;
}

function ok(val, text) {
  var status = val ? 'Test <font color=\\'green\\'>passed</font>: '
                   : 'Test <font color=\\'red\\'  >FAILED</font>: ';
  debug(status + text);
}

function todo(val, text) {
  var status = val ? 'Test <font color=\\'orange\\'>UNEXPECTED PASS</font>: '
                   : 'Test <font color=\\'blue\\'  >todo</font>: ';
  debug(status + text);
}

function addLoadEvent(func) {
  window.addEventListener('load', func);
}

SimpleTest = {
  waitForExplicitFinish: () => {},
  finish: () => {},
  requestFlakyTimeout: () => {},
};

SpecialPowers = {
  pushPrefEnv: async (env, callback = null) => {
    console.log(`SpecialPowers.pushPrefEnv(${JSON.stringify(env)})`);
    await new Promise(res => {
        setTimeout(res, 0);
    });
    if (callback) {
        await callback();
    }
  },
  popPrefEnv: async (callback = null) => {
    console.log('SpecialPowers.popPrefEnv()');
    await new Promise(res => {
        setTimeout(res, 0);
    });
    if (callback) {
        await callback();
    }
  },
};
</script>
<div id='mochi-to-testcase-output'></div>

"""

INCLUDE_PATTERN = re.compile(b"<script\\s*src=['\"](.*)\\.js['\"]>\\s*</script>")
CSS_PATTERN = re.compile(
    b"<link\\s*rel=['\"]stylesheet['\"]\\s*href=['\"]([^=>]*)['\"]>"
)

with open(TEST_PATH, "wb") as fout:
    with open(MOCHI_PATH, "rb") as fin:
        for line in fin:
            skip_line = False
            for css in CSS_PATTERN.findall(line):
                skip_line = True
                print("Ignoring stylesheet: " + css.decode())

            for inc in INCLUDE_PATTERN.findall(line):
                skip_line = True
                if inc == b"/MochiKit/MochiKit":
                    continue

                if inc == b"/tests/SimpleTest/SimpleTest":
                    print("Injecting SimpleTest replacement")
                    fout.write(SIMPLETEST_REPLACEMENT)
                    continue

                inc_js = inc.decode() + ".js"
                inc_data = read_local_file(inc_js)
                if not inc_data:
                    print("Warning: Unknown JS file ignored: " + inc_js)
                    continue

                print("Injecting include: " + inc_js)
                fout.write(b"\n<script>\n// Imported from: " + inc_js.encode() + b"\n")
                fout.write(inc_data)
                fout.write(b"\n</script>\n")
                continue

            if skip_line:
                continue

            fout.write(line)
            continue
