# UPGRADING

1. `git clone https://github.com/mozilla/pretty-fast.git`

2. Copy `pretty-fast/pretty-fast.js` to `devtools/shared/pretty-fast/pretty-fast.js`

3. Copy `pretty-fast/test.js` to `devtools/shared/pretty-fast/tests/unit/test.js`

4. If necessary, upgrade acorn (see devtools/shared/acorn/UPGRADING.md)

5. Replace `acorn/dist/` with `acorn/` in pretty-fast.js.