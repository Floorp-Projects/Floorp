/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* eslint-env worker */

"use strict";

importScripts("resource:///modules/ParseSymbols.jsm");

class WorkerCppFiltParser {
  constructor(length) {
    this._decoder = new TextDecoder();
    this._index = 0;
    this._approximateLength = 0;
    this._results = new Array(length);
  }

  consume(arrayBuffer) {
    const data = this._decoder.decode(arrayBuffer, {stream: true});
    const lineRegex = /.*\n?/g;
    const buffer = this._currentLine + data;

    let match;
    while ((match = lineRegex.exec(buffer))) {
      let [line] = match;
      if (line[line.length - 1] === "\n") {
        this._processLine(line);
      } else {
        this._currentLine = line;
        break;
      }
    }
  }

  finish() {
    this._processLine(this._currentLine);
    return {symsArray: this._results, approximateLength: this._approximateLength};
  }

  _processLine(line) {
    const trimmed = line.trimRight();
    this._approximateLength += trimmed.length;
    this._results[this._index++] = trimmed;
  }
}

let cppFiltParser;
onmessage = async e => {
  try {
    if (!cppFiltParser) {
      cppFiltParser = new WorkerCppFiltParser();
    }
    if (e.data.finish) {
      const {symsArray, approximateLength} = cppFiltParser.finish();
      const result = ParseSymbols.convertSymsArrayToExpectedSymFormat(symsArray, approximateLength);

      postMessage({result}, result.map(r => r.buffer));
      close();
    } else {
      cppFiltParser.consume(e.data.buffer);
    }
  } catch (error) {
    postMessage({error: error.toString()});
  }
};
