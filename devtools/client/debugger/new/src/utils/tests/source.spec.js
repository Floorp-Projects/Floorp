/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  getFilename,
  getTruncatedFileName,
  getFileURL,
  getDisplayPath,
  getMode,
  getSourceLineCount,
  isThirdParty,
  isJavaScript
} from "../source.js";

describe("sources", () => {
  const unicode = "\u6e2c";
  const encodedUnicode = encodeURIComponent(unicode);
  const punycode = "xn--g6w";

  describe("getFilename", () => {
    it("should give us a default of (index)", () => {
      expect(
        getFilename({ url: "http://localhost.com:7999/increment/", id: "" })
      ).toBe("(index)");
    });
    it("should give us the filename", () => {
      expect(
        getFilename({
          url: "http://localhost.com:7999/increment/hello.html",
          id: ""
        })
      ).toBe("hello.html");
    });
    it("should give us the readable Unicode filename if encoded", () => {
      expect(
        getFilename({
          url: `http://localhost.com:7999/increment/${encodedUnicode}.html`,
          id: ""
        })
      ).toBe(`${unicode}.html`);
    });
    it("should give us the filename excluding the query strings", () => {
      expect(
        getFilename({
          url: "http://localhost.com:7999/increment/hello.html?query_strings",
          id: ""
        })
      ).toBe("hello.html");
    });
    it("should give us the proper filename for pretty files", () => {
      expect(
        getFilename({
          url: "http://localhost.com:7999/increment/hello.html:formatted",
          id: ""
        })
      ).toBe("hello.html");
    });
  });

  describe("getTruncatedFileName", () => {
    it("should truncate the file name when it is more than 30 chars", () => {
      expect(
        getTruncatedFileName(
          {
            url: "really-really-really-really-really-really-long-name.html",
            id: ""
          },
          "",
          30
        )
      ).toBe("really-really…long-name.html");
    });
    it("should first decode the filename and then truncate it", () => {
      expect(
        getTruncatedFileName(
          {
            url: `${encodedUnicode.repeat(30)}.html`,
            id: ""
          },
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
          {
            url: "http://localhost.com:7999/increment/abc/hello.html",
            id: ""
          },
          [
            {
              url: "http://localhost.com:7999/increment/xyz/hello.html",
              id: ""
            },
            {
              url: "http://localhost.com:7999/increment/abc/hello.html",
              id: ""
            },
            {
              url: "http://localhost.com:7999/increment/hello.html",
              id: ""
            }
          ]
        )
      ).toBe("abc");
    });

    it(`should give us the path for files with same name
      in directories with same name`, () => {
      expect(
        getDisplayPath(
          {
            url: "http://localhost.com:7999/increment/abc/web/hello.html",
            id: ""
          },
          [
            {
              url: "http://localhost.com:7999/increment/xyz/web/hello.html",
              id: ""
            },
            {
              url: "http://localhost.com:7999/increment/abc/web/hello.html",
              id: ""
            },
            {
              url: "http://localhost.com:7999/increment/hello.html",
              id: ""
            }
          ]
        )
      ).toBe("abc/web");
    });

    it("should give no path for files with unique name", () => {
      expect(
        getDisplayPath(
          {
            url: "http://localhost.com:7999/increment/abc/web.html",
            id: ""
          },
          [
            {
              url: "http://localhost.com:7999/increment/xyz.html",
              id: ""
            },
            {
              url: "http://localhost.com:7999/increment/abc.html",
              id: ""
            },
            {
              url: "http://localhost.com:7999/increment/hello.html",
              id: ""
            }
          ]
        )
      ).toBe(undefined);
    });
    it("should not show display path for pretty file", () => {
      expect(
        getDisplayPath(
          {
            url:
              "http://localhost.com:7999/increment/abc/web/hello.html:formatted",
            id: ""
          },
          [
            {
              url: "http://localhost.com:7999/increment/abc/web/hell.html",
              id: ""
            },
            {
              url: "http://localhost.com:7999/increment/abc/web/hello.html",
              id: ""
            },
            {
              url: "http://localhost.com:7999/increment/xyz.html:formatted",
              id: ""
            }
          ]
        )
      ).toBe(undefined);
    });
    it(`should give us the path for files with same name when both
      are pretty and different path`, () => {
      expect(
        getDisplayPath(
          {
            url:
              "http://localhost.com:7999/increment/abc/web/hello.html:formatted",
            id: ""
          },
          [
            {
              url:
                "http://localhost.com:7999/increment/xyz/web/hello.html:formatted",
              id: ""
            },
            {
              url:
                "http://localhost.com:7999/increment/abc/web/hello.html:formatted",
              id: ""
            },
            {
              url: "http://localhost.com:7999/increment/hello.html:formatted",
              id: ""
            }
          ]
        )
      ).toBe("abc/web");
    });
  });

  describe("getFileURL", () => {
    it("should give us the file url", () => {
      expect(
        getFileURL({
          url: "http://localhost.com:7999/increment/hello.html",
          id: ""
        })
      ).toBe("http://localhost.com:7999/increment/hello.html");
    });
    it("should give us the readable Unicode file URL if encoded", () => {
      expect(
        getFileURL({
          url: `http://${punycode}.${punycode}:7999/increment/${encodedUnicode}.html`,
          id: ""
        })
      ).toBe(`http://${unicode}.${unicode}:7999/increment/${unicode}.html`);
    });
    it("should truncate the file url when it is more than 50 chars", () => {
      expect(
        getFileURL({
          url: "http://localhost-long.com:7999/increment/hello.html",
          id: ""
        })
      ).toBe("…ttp://localhost-long.com:7999/increment/hello.html");
    });
    it("should first decode the file URL and then truncate it", () => {
      expect(
        getFileURL({
          url: `http://${encodedUnicode.repeat(39)}.html`,
          id: ""
        })
      ).toBe(`…ttp://${unicode.repeat(39)}.html`);
    });
  });

  describe("isJavaScript", () => {
    it("is not JavaScript", () => {
      expect(isJavaScript({ url: "foo.html" })).toBe(false);
      expect(isJavaScript({ contentType: "text/html" })).toBe(false);
    });

    it("is JavaScript", () => {
      expect(isJavaScript({ url: "foo.js" })).toBe(true);
      expect(isJavaScript({ url: "bar.jsm" })).toBe(true);
      expect(isJavaScript({ contentType: "text/javascript" })).toBe(true);
      expect(isJavaScript({ contentType: "application/javascript" })).toBe(
        true
      );
    });
  });

  describe("isThirdParty", () => {
    it("node_modules", () => {
      expect(isThirdParty({ url: "/node_modules/foo.js" })).toBe(true);
    });

    it("bower_components", () => {
      expect(isThirdParty({ url: "/bower_components/foo.js" })).toBe(true);
    });

    it("not third party", () => {
      expect(isThirdParty({ url: "/bar/foo.js" })).toBe(false);
    });
  });

  describe("getMode", () => {
    it("//@flow", () => {
      const source = {
        contentType: "text/javascript",
        text: "// @flow",
        url: ""
      };
      expect(getMode(source)).toEqual({ name: "javascript", typescript: true });
    });

    it("/* @flow */", () => {
      const source = {
        contentType: "text/javascript",
        text: "   /* @flow */",
        url: ""
      };
      expect(getMode(source)).toEqual({ name: "javascript", typescript: true });
    });

    it("mixed html", () => {
      const source = {
        contentType: "",
        text: " <html",
        url: ""
      };
      expect(getMode(source)).toEqual({ name: "htmlmixed" });
    });

    it("elm", () => {
      const source = {
        contentType: "text/x-elm",
        text: 'main = text "Hello, World!"',
        url: ""
      };
      expect(getMode(source)).toEqual({ name: "elm" });
    });

    it("returns jsx if contentType jsx is given", () => {
      const source = {
        contentType: "text/jsx",
        text: "<h1></h1>",
        url: ""
      };
      expect(getMode(source)).toEqual({ name: "jsx" });
    });

    it("returns jsx if sourceMetaData says it's a react component", () => {
      const source = {
        text: "<h1></h1>",
        url: ""
      };
      expect(getMode(source, { hasJsx: true })).toEqual({ name: "jsx" });
    });

    it("returns jsx if the fileExtension is .jsx", () => {
      const source = {
        text: "<h1></h1>",
        url: "myComponent.jsx"
      };
      expect(getMode(source)).toEqual({ name: "jsx" });
    });

    it("returns text/x-haxe if the file extension is .hx", () => {
      const source = {
        text: "function foo(){}",
        url: "myComponent.hx"
      };
      expect(getMode(source)).toEqual({ name: "text/x-haxe" });
    });

    it("typescript", () => {
      const source = {
        contentType: "text/typescript",
        text: "function foo(){}",
        url: ""
      };
      expect(getMode(source)).toEqual({ name: "javascript", typescript: true });
    });

    it("typescript-jsx", () => {
      const source = {
        contentType: "text/typescript-jsx",
        text: "<h1></h1>",
        url: ""
      };

      expect(getMode(source).base.typescript).toBe(true);
    });

    it("cross-platform clojure(script) with reader conditionals", () => {
      const source = {
        contentType: "text/x-clojure",
        text:
          "(defn str->int [s] " +
          "  #?(:clj  (java.lang.Integer/parseInt s) " +
          "     :cljs (js/parseInt s)))",
        url: "my-clojurescript-source-with-reader-conditionals.cljc"
      };
      expect(getMode(source)).toEqual({ name: "clojure" });
    });

    it("clojurescript", () => {
      const source = {
        contentType: "text/x-clojurescript",
        text: "(+ 1 2 3)",
        url: "my-clojurescript-source.cljs"
      };
      expect(getMode(source)).toEqual({ name: "clojure" });
    });

    it("coffeescript", () => {
      const source = {
        contentType: "text/coffeescript",
        text: "x = (a) -> 3",
        url: ""
      };
      expect(getMode(source)).toEqual({ name: "coffeescript" });
    });

    it("wasm", () => {
      const source = {
        contentType: "",
        isWasm: true,
        text: {
          binary: "\x00asm\x01\x00\x00\x00"
        },
        url: ""
      };
      expect(getMode(source)).toEqual({ name: "text" });
    });

    it("marko", () => {
      const source = {
        contentType: "does not matter",
        text: "function foo(){}",
        url: "http://localhost.com:7999/increment/sometestfile.marko"
      };
      expect(getMode(source)).toEqual({ name: "javascript" });
    });
  });

  describe("getSourceLineCount", () => {
    it("should give us the amount bytes for wasm source", () => {
      const source = {
        contentType: "",
        isWasm: true,
        text: {
          binary: "\x00asm\x01\x00\x00\x00"
        }
      };
      expect(getSourceLineCount(source)).toEqual(8);
    });

    it("should give us the amout of lines for js source", () => {
      const source = {
        contentType: "text/javascript",
        text: "function foo(){\n}"
      };
      expect(getSourceLineCount(source)).toEqual(2);
    });
  });
});
