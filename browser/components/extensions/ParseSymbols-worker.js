/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* eslint-env worker */

"use strict";

importScripts("resource://gre/modules/osfile.jsm");
importScripts("resource:///modules/ParseSymbols.jsm");

async function fetchSymbolFile(url) {
  const response = await fetch(url);

  if (!response.ok) {
    throw new Error(`got error status ${response.status}`);
  }

  return response.text();
}

function parse(text) {
  const syms = new Map();

  // Lines look like this:
  //
  // PUBLIC 3fc74 0 test_public_symbol
  //
  // FUNC 40330 8e 0 test_func_symbol
  const symbolRegex = /\nPUBLIC ([0-9a-f]+) [0-9a-f]+ (.*)|\nFUNC ([0-9a-f]+) [0-9a-f]+ [0-9a-f]+ (.*)/g;

  let match;
  let approximateLength = 0;
  while ((match = symbolRegex.exec(text))) {
    const [address0, symbol0, address1, symbol1] = match.slice(1);
    const address = parseInt(address0 || address1, 16);
    const sym = (symbol0 || symbol1).trimRight();
    syms.set(address, sym);
    approximateLength += sym.length;
  }

  return ParseSymbols.convertSymsMapToExpectedSymFormat(syms, approximateLength);
}

onmessage = async e => {
  try {
    let text;
    if (e.data.filepath) {
      text = await OS.File.read(e.data.filepath, {encoding: "utf-8"});
    } else if (e.data.url) {
      text = await fetchSymbolFile(e.data.url);
    }

    const result = parse(text);
    postMessage({result}, result.map(r => r.buffer));
  } catch (error) {
    postMessage({error: error.toString()});
  }
  close();
};
