/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  getFilename,
  getTruncatedFileName,
  getFileURL,
  getDisplayPath,
  getMode,
  getSourceLineCount,
  isThirdParty,
  isJavaScript,
  isUrlExtension,
} from "../source.js";

import {
  makeMockSource,
  makeMockSourceWithContent,
  makeMockSourceAndContent,
  makeMockWasmSourceWithContent,
} from "../test-mockup";
import { isFulfilled } from "../async-value.js";

const defaultSymbolDeclarations = {
  classes: [],
  functions: [],
  memberExpressions: [],
  callExpressions: [],
  objectProperties: [],
  identifiers: [],
  imports: [],
  comments: [],
  literals: [],
  hasJsx: false,
  hasTypes: false,
  loading: false,
  framework: undefined,
};

describe("sources", () => {
  const unicode = "\u6e2c";
  const encodedUnicode = encodeURIComponent(unicode);
  const punycode = "xn--g6w";

  describe("getFilename", () => {
    it("should give us a default of (index)", () => {
      expect(
        getFilename(makeMockSource("http://localhost.com:7999/increment/"))
      ).toBe("(index)");
    });
    it("should give us the filename", () => {
      expect(
        getFilename(
          makeMockSource("http://localhost.com:7999/increment/hello.html")
        )
      ).toBe("hello.html");
    });
    it("should give us the readable Unicode filename if encoded", () => {
      expect(
        getFilename(
          makeMockSource(
            `http://localhost.com:7999/increment/${encodedUnicode}.html`
          )
        )
      ).toBe(`${unicode}.html`);
    });
    it("should give us the filename excluding the query strings", () => {
      expect(
        getFilename(
          makeMockSource(
            "http://localhost.com:7999/increment/hello.html?query_strings"
          )
        )
      ).toBe("hello.html");
    });
    it("should give us the proper filename for pretty files", () => {
      expect(
        getFilename(
          makeMockSource(
            "http://localhost.com:7999/increment/hello.html:formatted"
          )
        )
      ).toBe("hello.html");
    });
  });

  describe("getTruncatedFileName", () => {
    it("should truncate the file name when it is more than 30 chars", () => {
      expect(
        getTruncatedFileName(
          makeMockSource(
            "really-really-really-really-really-really-long-name.html"
          ),
          "",
          30
        )
      ).toBe("really-really…long-name.html");
    });
    it("should first decode the filename and then truncate it", () => {
      expect(
        getTruncatedFileName(
          makeMockSource(`${encodedUnicode.repeat(30)}.html`),
          "",
          30
        )
      ).toBe("測測測測測測測測測測測測測…測測測測測測測測測.html");
    });
  });

  describe("getDisplayPath", () => {
    it("should give us the path for files with same name", () => {
      expect(
        getDisplayPath(
          makeMockSource("http://localhost.com:7999/increment/abc/hello.html"),
          [
            makeMockSource(
              "http://localhost.com:7999/increment/xyz/hello.html"
            ),
            makeMockSource(
              "http://localhost.com:7999/increment/abc/hello.html"
            ),
            makeMockSource("http://localhost.com:7999/increment/hello.html"),
          ]
        )
      ).toBe("abc");
    });

    it(`should give us the path for files with same name
      in directories with same name`, () => {
      expect(
        getDisplayPath(
          makeMockSource(
            "http://localhost.com:7999/increment/abc/web/hello.html"
          ),
          [
            makeMockSource(
              "http://localhost.com:7999/increment/xyz/web/hello.html"
            ),
            makeMockSource(
              "http://localhost.com:7999/increment/abc/web/hello.html"
            ),
            makeMockSource("http://localhost.com:7999/increment/hello.html"),
          ]
        )
      ).toBe("abc/web");
    });

    it("should give no path for files with unique name", () => {
      expect(
        getDisplayPath(
          makeMockSource("http://localhost.com:7999/increment/abc/web.html"),
          [
            makeMockSource("http://localhost.com:7999/increment/xyz.html"),
            makeMockSource("http://localhost.com:7999/increment/abc.html"),
            makeMockSource("http://localhost.com:7999/increment/hello.html"),
          ]
        )
      ).toBe(undefined);
    });
    it("should not show display path for pretty file", () => {
      expect(
        getDisplayPath(
          makeMockSource(
            "http://localhost.com:7999/increment/abc/web/hello.html:formatted"
          ),
          [
            makeMockSource(
              "http://localhost.com:7999/increment/abc/web/hell.html"
            ),
            makeMockSource(
              "http://localhost.com:7999/increment/abc/web/hello.html"
            ),
            makeMockSource(
              "http://localhost.com:7999/increment/xyz.html:formatted"
            ),
          ]
        )
      ).toBe(undefined);
    });
    it(`should give us the path for files with same name when both
      are pretty and different path`, () => {
      expect(
        getDisplayPath(
          makeMockSource(
            "http://localhost.com:7999/increment/abc/web/hello.html:formatted"
          ),
          [
            makeMockSource(
              "http://localhost.com:7999/increment/xyz/web/hello.html:formatted"
            ),
            makeMockSource(
              "http://localhost.com:7999/increment/abc/web/hello.html:formatted"
            ),
            makeMockSource(
              "http://localhost.com:7999/increment/hello.html:formatted"
            ),
          ]
        )
      ).toBe("abc/web");
    });
  });

  describe("getFileURL", () => {
    it("should give us the file url", () => {
      expect(
        getFileURL(
          makeMockSource("http://localhost.com:7999/increment/hello.html")
        )
      ).toBe("http://localhost.com:7999/increment/hello.html");
    });
    it("should give us the readable Unicode file URL if encoded", () => {
      expect(
        getFileURL(
          makeMockSource(
            `http://${punycode}.${punycode}:7999/increment/${encodedUnicode}.html`
          )
        )
      ).toBe(`http://${unicode}.${unicode}:7999/increment/${unicode}.html`);
    });
    it("should truncate the file url when it is more than 50 chars", () => {
      expect(
        getFileURL(
          makeMockSource("http://localhost-long.com:7999/increment/hello.html")
        )
      ).toBe("…ttp://localhost-long.com:7999/increment/hello.html");
    });
    it("should first decode the file URL and then truncate it", () => {
      expect(
        getFileURL(makeMockSource(`http://${encodedUnicode.repeat(39)}.html`))
      ).toBe(`…ttp://${unicode.repeat(39)}.html`);
    });
  });

  describe("isJavaScript", () => {
    it("is not JavaScript", () => {
      {
        const { source, content } = makeMockSourceAndContent(
          "foo.html",
          undefined,
          ""
        );
        expect(isJavaScript(source, content)).toBe(false);
      }
      {
        const { source, content } = makeMockSourceAndContent(
          undefined,
          undefined,
          "text/html"
        );
        expect(isJavaScript(source, content)).toBe(false);
      }
    });

    it("is JavaScript", () => {
      {
        const { source, content } = makeMockSourceAndContent("foo.js");
        expect(isJavaScript(source, content)).toBe(true);
      }
      {
        const { source, content } = makeMockSourceAndContent("bar.jsm");
        expect(isJavaScript(source, content)).toBe(true);
      }
      {
        const { source, content } = makeMockSourceAndContent(
          undefined,
          undefined,
          "text/javascript"
        );
        expect(isJavaScript(source, content)).toBe(true);
      }
      {
        const { source, content } = makeMockSourceAndContent(
          undefined,
          undefined,
          "application/javascript"
        );
        expect(isJavaScript(source, content)).toBe(true);
      }
    });
  });

  describe("isThirdParty", () => {
    it("node_modules", () => {
      expect(isThirdParty(makeMockSource("/node_modules/foo.js"))).toBe(true);
    });

    it("bower_components", () => {
      expect(isThirdParty(makeMockSource("/bower_components/foo.js"))).toBe(
        true
      );
    });

    it("not third party", () => {
      expect(isThirdParty(makeMockSource("/bar/foo.js"))).toBe(false);
    });
  });

  describe("getMode", () => {
    it("//@flow", () => {
      const { source, content } = makeMockSourceAndContent(
        undefined,
        undefined,
        "text/javascript",
        "// @flow"
      );
      expect(getMode(source, content)).toEqual({
        name: "javascript",
        typescript: true,
      });
    });

    it("/* @flow */", () => {
      const { source, content } = makeMockSourceAndContent(
        undefined,
        undefined,
        "text/javascript",
        "   /* @flow */"
      );
      expect(getMode(source, content)).toEqual({
        name: "javascript",
        typescript: true,
      });
    });

    it("mixed html", () => {
      const { source, content } = makeMockSourceAndContent(
        undefined,
        undefined,
        "",
        " <html"
      );
      expect(getMode(source, content)).toEqual({ name: "htmlmixed" });
    });

    it("elm", () => {
      const { source, content } = makeMockSourceAndContent(
        undefined,
        undefined,
        "text/x-elm",
        'main = text "Hello, World!"'
      );
      expect(getMode(source, content)).toEqual({ name: "elm" });
    });

    it("returns jsx if contentType jsx is given", () => {
      const { source, content } = makeMockSourceAndContent(
        undefined,
        undefined,
        "text/jsx",
        "<h1></h1>"
      );
      expect(getMode(source, content)).toEqual({ name: "jsx" });
    });

    it("returns jsx if sourceMetaData says it's a react component", () => {
      const { source, content } = makeMockSourceAndContent(
        undefined,
        undefined,
        "",
        "<h1></h1>"
      );
      expect(
        getMode(source, content, { ...defaultSymbolDeclarations, hasJsx: true })
      ).toEqual({ name: "jsx" });
    });

    it("returns jsx if the fileExtension is .jsx", () => {
      const { source, content } = makeMockSourceAndContent(
        "myComponent.jsx",
        undefined,
        "",
        "<h1></h1>"
      );
      expect(getMode(source, content)).toEqual({ name: "jsx" });
    });

    it("returns text/x-haxe if the file extension is .hx", () => {
      const { source, content } = makeMockSourceAndContent(
        "myComponent.hx",
        undefined,
        "",
        "function foo(){}"
      );
      expect(getMode(source, content)).toEqual({ name: "text/x-haxe" });
    });

    it("typescript", () => {
      const { source, content } = makeMockSourceAndContent(
        undefined,
        undefined,
        "text/typescript",
        "function foo(){}"
      );
      expect(getMode(source, content)).toEqual({
        name: "javascript",
        typescript: true,
      });
    });

    it("typescript-jsx", () => {
      const { source, content } = makeMockSourceAndContent(
        undefined,
        undefined,
        "text/typescript-jsx",
        "<h1></h1>"
      );
      expect(getMode(source, content).base).toEqual({
        name: "javascript",
        typescript: true,
      });
    });

    it("cross-platform clojure(script) with reader conditionals", () => {
      const { source, content } = makeMockSourceAndContent(
        "my-clojurescript-source-with-reader-conditionals.cljc",
        undefined,
        "text/x-clojure",
        "(defn str->int [s] " +
          "  #?(:clj  (java.lang.Integer/parseInt s) " +
          "     :cljs (js/parseInt s)))"
      );
      expect(getMode(source, content)).toEqual({ name: "clojure" });
    });

    it("clojurescript", () => {
      const { source, content } = makeMockSourceAndContent(
        "my-clojurescript-source.cljs",
        undefined,
        "text/x-clojurescript",
        "(+ 1 2 3)"
      );
      expect(getMode(source, content)).toEqual({ name: "clojure" });
    });

    it("coffeescript", () => {
      const { source, content } = makeMockSourceAndContent(
        undefined,
        undefined,
        "text/coffeescript",
        "x = (a) -> 3"
      );
      expect(getMode(source, content)).toEqual({ name: "coffeescript" });
    });

    it("wasm", () => {
      const { source, content } = makeMockWasmSourceWithContent({
        binary: "\x00asm\x01\x00\x00\x00",
      });
      expect(getMode(source, content.value)).toEqual({ name: "text" });
    });

    it("marko", () => {
      const { source, content } = makeMockSourceAndContent(
        "http://localhost.com:7999/increment/sometestfile.marko",
        undefined,
        "does not matter",
        "function foo(){}"
      );
      expect(getMode(source, content)).toEqual({ name: "javascript" });
    });
  });

  describe("getSourceLineCount", () => {
    it("should give us the amount bytes for wasm source", () => {
      const { content } = makeMockWasmSourceWithContent({
        binary: "\x00asm\x01\x00\x00\x00",
      });
      expect(getSourceLineCount(content.value)).toEqual(8);
    });

    it("should give us the amout of lines for js source", () => {
      const { content } = makeMockSourceWithContent(
        undefined,
        undefined,
        "text/javascript",
        "function foo(){\n}"
      );
      if (!content || !isFulfilled(content)) {
        throw new Error("Unexpected content value");
      }
      expect(getSourceLineCount(content.value)).toEqual(2);
    });
  });

  describe("isUrlExtension", () => {
    it("should detect mozilla extenstion", () => {
      expect(isUrlExtension("moz-extension://id/js/content.js")).toBe(true);
    });
    it("should detect chrome extenstion", () => {
      expect(isUrlExtension("chrome-extension://id/js/content.js")).toBe(true);
    });
    it("should return false for non-extension assets", () => {
      expect(isUrlExtension("https://example.org/init.js")).toBe(false);
    });
  });
});
