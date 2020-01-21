/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test devtools/client/netmonitor/src/utils/request-utils.js function
// |getFormattedProtocol|

"use strict";

const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");
const {
  getFormattedProtocol,
} = require("devtools/client/netmonitor/src/utils/request-utils");

function run_test() {
  const http_1p1_value_http1p1 = {
    httpVersion: "HTTP/1.1",
    responseHeaders: {
      headers: [
        {
          name: "X-Firefox-Spdy",
          value: "http/1.1",
        },
      ],
    },
  };

  const http_1p1_value_http_no_slash_1p1 = {
    httpVersion: "HTTP/1.1",
    responseHeaders: {
      headers: [
        {
          name: "X-Firefox-Spdy",
          value: "http1.1",
        },
      ],
    },
  };

  const http_1p1_value_http1p11 = {
    httpVersion: "HTTP/1.1",
    responseHeaders: {
      headers: [
        {
          name: "X-Firefox-Spdy",
          value: "http/1.11",
        },
      ],
    },
  };

  const http_2p0_value_h2 = {
    httpVersion: "HTTP/2.0",
    responseHeaders: {
      headers: [
        {
          name: "X-Firefox-Spdy",
          value: "h2",
        },
      ],
    },
  };

  const http_1p1_value_h1 = {
    httpVersion: "HTTP/1.1",
    responseHeaders: {
      headers: [
        {
          name: "X-Firefox-Spdy",
          value: "h1",
        },
      ],
    },
  };

  const http_1p1_value_h2 = {
    httpVersion: "HTTP/1.1",
    responseHeaders: {
      headers: [
        {
          name: "X-Firefox-Spdy",
          value: "h2",
        },
      ],
    },
  };

  const http_1p1_value_empty_string = {
    httpVersion: "HTTP/1.1",
    responseHeaders: {
      headers: [
        {
          name: "X-Firefox-Spdy",
          value: "",
        },
      ],
    },
  };

  const http_2p0_value_empty_string = {
    httpVersion: "HTTP/2.0",
    responseHeaders: {
      headers: [
        {
          name: "X-Firefox-Spdy",
          value: "",
        },
      ],
    },
  };

  const http_2p0_value_2p0 = {
    httpVersion: "HTTP/2.0",
    responseHeaders: {
      headers: [
        {
          name: "X-Firefox-Spdy",
          value: "HTTP/2.0",
        },
      ],
    },
  };

  const http_3p0_value_h3 = {
    httpVersion: "HTTP/3.0",
    responseHeaders: {
      headers: [
        {
          name: "X-Firefox-Spdy",
          value: "h3",
        },
      ],
    },
  };

  const http_3p0_value_h3p0 = {
    httpVersion: "HTTP/3.0",
    responseHeaders: {
      headers: [
        {
          name: "X-Firefox-Spdy",
          value: "h3.0",
        },
      ],
    },
  };

  const http_3p0_value_http_3p0 = {
    httpVersion: "HTTP/3.0",
    responseHeaders: {
      headers: [
        {
          name: "X-Firefox-Spdy",
          value: "http/3.0",
        },
      ],
    },
  };

  const http_3p0_value_3p0 = {
    httpVersion: "HTTP/3.0",
    responseHeaders: {
      headers: [
        {
          name: "X-Firefox-Spdy",
          value: "3.0",
        },
      ],
    },
  };

  const http_4p0_value_h4 = {
    httpVersion: "HTTP/4.0",
    responseHeaders: {
      headers: [
        {
          name: "X-Firefox-Spdy",
          value: "h4",
        },
      ],
    },
  };

  info("Testing httpValue:HTTP/1.1, value:http/1.1");
  equal(getFormattedProtocol(http_1p1_value_http1p1), "HTTP/1.1");

  info("Testing httpValue:HTTP/1.1, value:http1.1");
  equal(
    getFormattedProtocol(http_1p1_value_http_no_slash_1p1),
    "HTTP/1.1+http1.1"
  );

  info("Testing httpValue:HTTP/1.1, value:http/1.11");
  equal(getFormattedProtocol(http_1p1_value_http1p11), "HTTP/1.1+http/1.11");

  info("Testing httpValue:HTTP/2.0, value:h2");
  equal(getFormattedProtocol(http_2p0_value_h2), "HTTP/2.0");

  info("Testing httpValue:HTTP/1.1, value:h1");
  equal(getFormattedProtocol(http_1p1_value_h1), "HTTP/1.1+h1");

  info("Testing httpValue:HTTP/1.1, value:h2");
  equal(getFormattedProtocol(http_1p1_value_h2), "HTTP/1.1+h2");

  info("Testing httpValue:HTTP/1.1, value:http1.1");
  equal(
    getFormattedProtocol(http_1p1_value_http_no_slash_1p1),
    "HTTP/1.1+http1.1"
  );

  info("Testing httpValue:HTTP/1.1, value:''");
  equal(getFormattedProtocol(http_1p1_value_empty_string), "HTTP/1.1");

  info("Testing httpValue:HTTP/2.0, value:''");
  equal(getFormattedProtocol(http_2p0_value_empty_string), "HTTP/2.0");

  info("Testing httpValue:HTTP/2.0, value:HTTP/2.0");
  equal(getFormattedProtocol(http_2p0_value_2p0), "HTTP/2.0+HTTP/2.0");

  info("Testing httpValue:HTTP/3.0, value:h3");
  equal(getFormattedProtocol(http_3p0_value_h3), "HTTP/3.0");

  info("Testing httpValue:HTTP/3.0, value:h3.0");
  equal(getFormattedProtocol(http_3p0_value_h3p0), "HTTP/3.0");

  info("Testing httpValue:HTTP/3.0, value:http/3.0");
  equal(getFormattedProtocol(http_3p0_value_http_3p0), "HTTP/3.0+http/3.0");

  info("Testing httpValue:HTTP/3.0, value:3.0");
  equal(getFormattedProtocol(http_3p0_value_3p0), "HTTP/3.0+3.0");

  info("Testing httpValue:HTTP/4.0, value:h4");
  equal(getFormattedProtocol(http_4p0_value_h4), "HTTP/4.0");
}
