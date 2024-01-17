/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getMode } from "../source-documents.js";

import {
  makeMockSourceWithContent,
  makeMockWasmSourceWithContent,
} from "../../test-mockup";

const defaultSymbolDeclarations = {
  classes: [],
  functions: [],
  memberExpressions: [],
  objectProperties: [],
  identifiers: [],
  comments: [],
  literals: [],
  hasJsx: false,
  hasTypes: false,
};

describe("source-documents", () => {
  describe("getMode", () => {
    it("//", () => {
      const source = makeMockSourceWithContent(
        undefined,
        undefined,
        "text/javascript",
        "// @flow"
      );
      expect(getMode(source, source.content)).toEqual({
        name: "javascript",
        typescript: true,
      });
    });

    it("/* @flow */", () => {
      const source = makeMockSourceWithContent(
        undefined,
        undefined,
        "text/javascript",
        "   /* @flow */"
      );
      expect(getMode(source, source.content)).toEqual({
        name: "javascript",
        typescript: true,
      });
    });

    it("mixed html", () => {
      const source = makeMockSourceWithContent(
        undefined,
        undefined,
        "",
        " <html"
      );
      expect(getMode(source, source.content)).toEqual({ name: "htmlmixed" });
    });

    it("elm", () => {
      const source = makeMockSourceWithContent(
        undefined,
        undefined,
        "text/x-elm",
        'main = text "Hello, World!"'
      );
      expect(getMode(source, source.content)).toEqual({ name: "elm" });
    });

    it("returns jsx if contentType jsx is given", () => {
      const source = makeMockSourceWithContent(
        undefined,
        undefined,
        "text/jsx",
        "<h1></h1>"
      );
      expect(getMode(source, source.content)).toEqual({ name: "jsx" });
    });

    it("returns jsx if sourceMetaData says it's a react component", () => {
      const source = makeMockSourceWithContent(
        undefined,
        undefined,
        "",
        "<h1></h1>"
      );
      expect(
        getMode(source, source.content, {
          ...defaultSymbolDeclarations,
          hasJsx: true,
        })
      ).toEqual({ name: "jsx" });
    });

    it("returns jsx if the fileExtension is .jsx", () => {
      const source = makeMockSourceWithContent(
        "myComponent.jsx",
        undefined,
        "",
        "<h1></h1>"
      );
      expect(getMode(source, source.content)).toEqual({ name: "jsx" });
    });

    it("returns text/x-haxe if the file extension is .hx", () => {
      const source = makeMockSourceWithContent(
        "myComponent.hx",
        undefined,
        "",
        "function foo(){}"
      );
      expect(getMode(source, source.content)).toEqual({ name: "text/x-haxe" });
    });

    it("typescript", () => {
      const source = makeMockSourceWithContent(
        undefined,
        undefined,
        "text/typescript",
        "function foo(){}"
      );
      expect(getMode(source, source.content)).toEqual({
        name: "javascript",
        typescript: true,
      });
    });

    it("typescript-jsx", () => {
      const source = makeMockSourceWithContent(
        undefined,
        undefined,
        "text/typescript-jsx",
        "<h1></h1>"
      );
      expect(getMode(source, source.content).base).toEqual({
        name: "javascript",
        typescript: true,
      });
    });

    it("cross-platform clojure(script) with reader conditionals", () => {
      const source = makeMockSourceWithContent(
        "my-clojurescript-source-with-reader-conditionals.cljc",
        undefined,
        "text/x-clojure",
        "(defn str->int [s] " +
          "  #?(:clj  (java.lang.Integer/parseInt s) " +
          "     :cljs (js/parseInt s)))"
      );
      expect(getMode(source, source.content)).toEqual({ name: "clojure" });
    });

    it("clojurescript", () => {
      const source = makeMockSourceWithContent(
        "my-clojurescript-source.cljs",
        undefined,
        "text/x-clojurescript",
        "(+ 1 2 3)"
      );
      expect(getMode(source, source.content)).toEqual({ name: "clojure" });
    });

    it("coffeescript", () => {
      const source = makeMockSourceWithContent(
        undefined,
        undefined,
        "text/coffeescript",
        "x = (a) -> 3"
      );
      expect(getMode(source, source.content)).toEqual({ name: "coffeescript" });
    });

    it("wasm", () => {
      const source = makeMockWasmSourceWithContent({
        binary: "\x00asm\x01\x00\x00\x00",
      });
      expect(getMode(source, source.content.value)).toEqual({ name: "text" });
    });

    it("marko", () => {
      const source = makeMockSourceWithContent(
        "http://localhost.com:7999/increment/sometestfile.marko",
        undefined,
        "does not matter",
        "function foo(){}"
      );
      expect(getMode(source, source.content)).toEqual({ name: "javascript" });
    });

    it("es6", () => {
      const source = makeMockSourceWithContent(
        "http://localhost.com:7999/increment/sometestfile.es6",
        undefined,
        "does not matter",
        "function foo(){}"
      );
      expect(getMode(source, source.content)).toEqual({ name: "javascript" });
    });

    it("vue", () => {
      const source = makeMockSourceWithContent(
        "http://localhost.com:7999/increment/sometestfile.vue?query=string",
        undefined,
        "does not matter",
        "function foo(){}"
      );
      expect(getMode(source, source.content)).toEqual({ name: "javascript" });
    });
  });
});
