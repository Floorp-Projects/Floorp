/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* eslint-env worker */

"use strict";

importScripts("resource:///modules/ParseSymbols.jsm");

class WorkerNMParser {
  constructor() {
    this._decoder = new TextDecoder();
    this._addrToSymMap = new Map();
    this._approximateLength = 0;
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
    return {syms: this._addrToSymMap, approximateLength: this._approximateLength};
  }

  _processLine(line) {
    // Example lines:
    // 00000000028c9888 t GrFragmentProcessor::MulOutputByInputUnpremulColor(sk_sp<GrFragmentProcessor>)::PremulFragmentProcessor::onCreateGLSLInstance() const::GLFP::~GLFP()
    // 00000000028c9874 t GrFragmentProcessor::MulOutputByInputUnpremulColor(sk_sp<GrFragmentProcessor>)::PremulFragmentProcessor::onCreateGLSLInstance() const::GLFP::~GLFP()
    // 00000000028c9874 t GrFragmentProcessor::MulOutputByInputUnpremulColor(sk_sp<GrFragmentProcessor>)::PremulFragmentProcessor::onCreateGLSLInstance() const::GLFP::~GLFP()
    // 0000000003a33730 r mozilla::OggDemuxer::~OggDemuxer()::{lambda()#1}::operator()() const::__func__
    // 0000000003a33930 r mozilla::VPXDecoder::Drain()::{lambda()#1}::operator()() const::__func__
    //
    // Some lines have the form
    // <address> ' ' <letter> ' ' <symbol>
    // and some have the form
    // <address> ' ' <symbol>
    // The letter has a meaning, but we ignore it.

    const regex = /([^ ]+) (?:. )?(.*)/;
    let match = regex.exec(line);
    if (match) {
      const [, address, symbol] = match;
      this._addrToSymMap.set(parseInt(address, 16), symbol);
      this._approximateLength += symbol.length;
    }
  }
}

let nmParser;
onmessage = async e => {
  try {
    if (!nmParser) {
      nmParser = new WorkerNMParser();
    }
    if (e.data.finish) {
      const {syms, approximateLength} = nmParser.finish();
      let result;
      if (e.data.isDarwin) {
        result = ParseSymbols.convertSymsMapToDemanglerFormat(syms);
      } else {
        result = ParseSymbols.convertSymsMapToExpectedSymFormat(syms, approximateLength);
      }

      postMessage({result}, result.map(r => r.buffer));
      close();
    } else {
      nmParser.consume(e.data.buffer);
    }
  } catch (error) {
    postMessage({error: error.toString()});
  }
};
