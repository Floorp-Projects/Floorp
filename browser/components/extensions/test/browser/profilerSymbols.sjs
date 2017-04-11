/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const SYMBOLS_FILE =
`MODULE windows x86_64 A712ED458B2542C18785C19D17D64D842 test.pdb

INFO CODE_ID 58EE0F7F3EDD000 test.dll

PUBLIC 3fc74 0 test_public_symbol

FUNC 40330 8e 0 test_func_symbol

40330 42 22 2229

40372 3a 23 2229

403ac 12 23 2229
`;

function handleRequest(req, resp) {
  let match = /path=([^\/]+)\/([^\/]+)\/([^\/]+)/.exec(req.queryString);
  if (match && match[1] == "test.pdb") {
    resp.write(SYMBOLS_FILE);
  } else {
    resp.setStatusLine(null, 404, 'Not Found');
  }
}
