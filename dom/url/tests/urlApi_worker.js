/* eslint-env worker */

function ok(a, msg) {
  dump("OK: " + !!a + "  =>  " + a + " " + msg + "\n");
  postMessage({ type: "status", status: !!a, msg: a + ": " + msg });
}

function is(a, b, msg) {
  dump("IS: " + (a === b) + "  =>  " + a + " | " + b + " " + msg + "\n");
  postMessage({
    type: "status",
    status: a === b,
    msg: a + " === " + b + ": " + msg,
  });
}

// eslint-disable-next-line complexity
onmessage = function () {
  let status = false;
  try {
    if (URL instanceof Object) {
      status = true;
    }
  } catch (e) {}

  ok(status, "URL in workers \\o/");

  var tests = [
    {
      url: "http://www.abc.com",
      base: undefined,
      error: false,
      href: "http://www.abc.com/",
      origin: "http://www.abc.com",
      protocol: "http:",
      username: "",
      password: "",
      host: "www.abc.com",
      hostname: "www.abc.com",
      port: "",
      pathname: "/",
      search: "",
      hash: "",
    },
    {
      url: "ftp://auser:apw@www.abc.com",
      base: undefined,
      error: false,
      href: "ftp://auser:apw@www.abc.com/",
      origin: "ftp://www.abc.com",
      protocol: "ftp:",
      username: "auser",
      password: "apw",
      host: "www.abc.com",
      hostname: "www.abc.com",
      port: "",
      pathname: "/",
      search: "",
      hash: "",
    },
    {
      url: "http://www.abc.com:90/apath/",
      base: undefined,
      error: false,
      href: "http://www.abc.com:90/apath/",
      origin: "http://www.abc.com:90",
      protocol: "http:",
      username: "",
      password: "",
      host: "www.abc.com:90",
      hostname: "www.abc.com",
      port: "90",
      pathname: "/apath/",
      search: "",
      hash: "",
    },
    {
      url: "http://www.abc.com/apath/afile.txt#ahash",
      base: undefined,
      error: false,
      href: "http://www.abc.com/apath/afile.txt#ahash",
      origin: "http://www.abc.com",
      protocol: "http:",
      username: "",
      password: "",
      host: "www.abc.com",
      hostname: "www.abc.com",
      port: "",
      pathname: "/apath/afile.txt",
      search: "",
      hash: "#ahash",
    },
    {
      url: "http://example.com/?test#hash",
      base: undefined,
      error: false,
      href: "http://example.com/?test#hash",
      origin: "http://example.com",
      protocol: "http:",
      username: "",
      password: "",
      host: "example.com",
      hostname: "example.com",
      port: "",
      pathname: "/",
      search: "?test",
      hash: "#hash",
    },
    {
      url: "http://example.com/?test",
      base: undefined,
      error: false,
      href: "http://example.com/?test",
      origin: "http://example.com",
      protocol: "http:",
      username: "",
      password: "",
      host: "example.com",
      hostname: "example.com",
      port: "",
      pathname: "/",
      search: "?test",
      hash: "",
    },
    {
      url: "http://example.com/carrot#question%3f",
      base: undefined,
      error: false,
      hash: "#question%3f",
    },
    {
      url: "https://example.com:4443?",
      base: undefined,
      error: false,
      protocol: "https:",
      port: "4443",
      pathname: "/",
      hash: "",
      search: "",
    },
    {
      url: "http://www.abc.com/apath/afile.txt#ahash?asearch",
      base: undefined,
      error: false,
      href: "http://www.abc.com/apath/afile.txt#ahash?asearch",
      protocol: "http:",
      pathname: "/apath/afile.txt",
      hash: "#ahash?asearch",
      search: "",
    },
    {
      url: "http://www.abc.com/apath/afile.txt?asearch#ahash",
      base: undefined,
      error: false,
      href: "http://www.abc.com/apath/afile.txt?asearch#ahash",
      protocol: "http:",
      pathname: "/apath/afile.txt",
      hash: "#ahash",
      search: "?asearch",
    },
    {
      url: "http://abc.com/apath/afile.txt?#ahash",
      base: undefined,
      error: false,
      pathname: "/apath/afile.txt",
      hash: "#ahash",
      search: "",
    },
    {
      url: "http://auser:apassword@www.abc.com:90/apath/afile.txt?asearch#ahash",
      base: undefined,
      error: false,
      protocol: "http:",
      username: "auser",
      password: "apassword",
      host: "www.abc.com:90",
      hostname: "www.abc.com",
      port: "90",
      pathname: "/apath/afile.txt",
      hash: "#ahash",
      search: "?asearch",
      origin: "http://www.abc.com:90",
    },

    { url: "/foo#bar", base: "www.test.org", error: true },
    { url: "/foo#bar", base: null, error: true },
    { url: "/foo#bar", base: 42, error: true },
    {
      url: "ftp://ftp.something.net",
      base: undefined,
      error: false,
      protocol: "ftp:",
    },
    {
      url: "file:///tmp/file",
      base: undefined,
      error: false,
      protocol: "file:",
    },
    {
      url: "gopher://gopher.something.net",
      base: undefined,
      error: false,
      protocol: "gopher:",
      expectedChangedProtocol: "https:",
    },
    {
      url: "ws://ws.something.net",
      base: undefined,
      error: false,
      protocol: "ws:",
    },
    {
      url: "wss://ws.something.net",
      base: undefined,
      error: false,
      protocol: "wss:",
    },
    {
      url: "foo://foo.something.net",
      base: undefined,
      error: false,
      protocol: "foo:",
      expectedChangedProtocol: "https:",
    },
  ];

  while (tests.length) {
    var test = tests.shift();

    var error = false;
    var url;
    try {
      if (test.base) {
        url = new URL(test.url, test.base);
      } else {
        url = new URL(test.url);
      }
    } catch (e) {
      error = true;
    }

    is(test.error, error, "Error creating URL");
    if (test.error) {
      continue;
    }

    if ("href" in test) {
      is(url.href, test.href, "href");
    }
    if ("origin" in test) {
      is(url.origin, test.origin, "origin");
    }
    if ("protocol" in test) {
      is(url.protocol, test.protocol, "protocol");
    }
    if ("username" in test) {
      is(url.username, test.username, "username");
    }
    if ("password" in test) {
      is(url.password, test.password, "password");
    }
    if ("host" in test) {
      is(url.host, test.host, "host");
    }
    if ("hostname" in test) {
      is(url.hostname, test.hostname, "hostname");
    }
    if ("port" in test) {
      is(url.port, test.port, "port");
    }
    if ("pathname" in test) {
      is(url.pathname, test.pathname, "pathname");
    }
    if ("search" in test) {
      is(url.search, test.search, "search");
    }
    if ("hash" in test) {
      is(url.hash, test.hash, "hash");
    }

    url = new URL("https://www.example.net/what#foo?bar");
    ok(url, "Url exists!");

    if ("href" in test) {
      url.href = test.href;
    }
    if ("protocol" in test) {
      url.protocol = test.protocol;
    }
    if ("username" in test && test.username) {
      url.username = test.username;
    }
    if ("password" in test && test.password) {
      url.password = test.password;
    }
    if ("host" in test) {
      url.host = test.host;
    }
    if ("hostname" in test) {
      url.hostname = test.hostname;
    }
    if ("port" in test) {
      url.port = test.port;
    }
    if ("pathname" in test) {
      url.pathname = test.pathname;
    }
    if ("search" in test) {
      url.search = test.search;
    }
    if ("hash" in test) {
      url.hash = test.hash;
    }

    if ("href" in test) {
      is(url.href, test.href, "href");
    }
    if ("origin" in test) {
      is(url.origin, test.origin, "origin");
    }
    if ("expectedChangedProtocol" in test) {
      is(url.protocol, test.expectedChangedProtocol, "protocol");
    } else if ("protocol" in test) {
      is(url.protocol, test.protocol, "protocol");
    }
    if ("username" in test) {
      is(url.username, test.username, "username");
    }
    if ("password" in test) {
      is(url.password, test.password, "password");
    }
    if ("host" in test) {
      is(url.host, test.host, "host");
    }
    if ("hostname" in test) {
      is(test.hostname, url.hostname, "hostname");
    }
    if ("port" in test) {
      is(test.port, url.port, "port");
    }
    if ("pathname" in test) {
      is(test.pathname, url.pathname, "pathname");
    }
    if ("search" in test) {
      is(test.search, url.search, "search");
    }
    if ("hash" in test) {
      is(test.hash, url.hash, "hash");
    }

    if ("href" in test) {
      is(test.href, url + "", "stringify works");
    }
  }

  postMessage({ type: "finish" });
};
