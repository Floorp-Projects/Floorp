#!/usr/bin/env python3
"""mozlz4.py — decompress Mozilla mozLz40 (.jsonlz4) files. Zero dependencies.

Usage:
  python3 mozlz4.py <file.jsonlz4>            # write JSON to stdout
  python3 mozlz4.py <file.jsonlz4> --out x.json
  python3 mozlz4.py <file.jsonlz4> --keys     # summary: top-level keys + sizes

mozLz40 format: 8-byte magic "mozLz40\\0" followed by a raw LZ4 block stream.
The LZ4 block format is self-terminating at end of input, so no uncompressed
size is needed when decoding the whole stream.
"""

import json
import struct
import sys

MAGIC = b"mozLz40\x00"


def lz4_block_decompress(data: bytes) -> bytes:
    """Decode a raw LZ4 block stream (no frame header) until input exhaustion."""
    out = bytearray()
    i = 0
    n = len(data)
    while i < n:
        token = data[i]
        i += 1
        # literal length
        lit_len = token >> 4
        if lit_len == 15:
            while True:
                if i >= n:
                    raise ValueError("truncated literal length")
                b = data[i]
                i += 1
                lit_len += b
                if b != 255:
                    break
        if i + lit_len > n:
            raise ValueError("truncated literals")
        out += data[i : i + lit_len]
        i += lit_len
        if i >= n:
            # last sequence: literals only, no match copy
            break
        # match offset (2 bytes LE) — 0 is invalid in LZ4
        if i + 2 > n:
            raise ValueError("truncated match offset")
        (offset,) = struct.unpack_from("<H", data, i)
        i += 2
        if offset == 0:
            raise ValueError("invalid zero offset")
        # match length
        match_len = token & 0x0F
        if match_len == 15:
            while True:
                if i >= n:
                    raise ValueError("truncated match length")
                b = data[i]
                i += 1
                match_len += b
                if b != 255:
                    break
        match_len += 4
        start = len(out) - offset
        if start < 0:
            raise ValueError("match offset before start of output")
        for k in range(match_len):
            out.append(out[start + k])
    return bytes(out)


def decompress_mozlz4(path: str) -> bytes:
    raw = open(path, "rb").read()
    if raw[:8] != MAGIC:
        raise ValueError(f"not a mozLz40 file (magic={raw[:8]!r})")
    if len(raw) < 12:
        raise ValueError("truncated header")
    # mozlz40 layout: magic(8) + decompressed size uint32 LE(4) + raw lz4 block stream
    (expected_size,) = struct.unpack_from("<I", raw, 8)
    plain = lz4_block_decompress(raw[12:])
    if len(plain) != expected_size:
        raise ValueError(
            f"size mismatch: header says {expected_size}, decoded {len(plain)}"
        )
    return plain


def main() -> int:
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if not args:
        print(__doc__)
        return 2
    path = args[0]
    try:
        plain = decompress_mozlz4(path)
    except Exception as e:  # noqa: BLE001
        print(f"ERROR: {e}", file=sys.stderr)
        return 1

    if "--out" in sys.argv:
        idx = sys.argv.index("--out")
        out_path = sys.argv[idx + 1]
        with open(out_path, "wb") as f:
            f.write(plain)
        print(json.dumps({"written": out_path, "bytes": len(plain)}))
        return 0

    if "--keys" in sys.argv:
        try:
            doc = json.loads(plain)
            summary = {
                "bytes": len(plain),
                "type": type(doc).__name__,
            }
            if isinstance(doc, dict):
                summary["keys"] = sorted(doc.keys())
                for k, v in doc.items():
                    if isinstance(v, (list, dict)):
                        summary[f"{k}.count"] = len(v)
            print(json.dumps(summary, ensure_ascii=False))
        except Exception as e:  # noqa: BLE001
            print(json.dumps({"bytes": len(plain), "parse_error": str(e)}))
        return 0

    sys.stdout.buffer.write(plain)
    if not plain.endswith(b"\n"):
        sys.stdout.buffer.write(b"\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
