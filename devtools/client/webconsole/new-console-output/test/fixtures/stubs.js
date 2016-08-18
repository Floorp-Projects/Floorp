/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  MESSAGE_SOURCE,
  MESSAGE_TYPE,
  MESSAGE_LEVEL,
} = require("devtools/client/webconsole/new-console-output/constants");

const { ConsoleMessage } = require("devtools/client/webconsole/new-console-output/types");

let stubConsoleMessages = new Map();

stubConsoleMessages.set("console.log('foobar', 'test')", new ConsoleMessage({
	"id": "1",
	"allowRepeating": true,
	"source": "console-api",
	"type": "log",
	"level": "log",
	"messageText": null,
	"parameters": [
		"foobar",
		"test"
	],
	"repeat": 1,
	"repeatId": "{\"id\":null,\"allowRepeating\":true,\"source\":\"console-api\",\"type\":\"log\",\"level\":\"log\",\"messageText\":null,\"parameters\":[\"foobar\",\"test\"],\"repeatId\":null,\"stacktrace\":null,\"frame\":{\"source\":\"http://example.com/browser/devtools/client/webconsole/new-console-output/test/fixtures/stub-generators/test-tempfile.js\",\"line\":1,\"column\":27}}",
	"stacktrace": null,
	"frame": {
		"source": "http://example.com/browser/devtools/client/webconsole/new-console-output/test/fixtures/stub-generators/test-tempfile.js",
		"line": 1,
		"column": 27
	}
}));

stubConsoleMessages.set("console.log(undefined)", new ConsoleMessage({
	"id": "1",
	"allowRepeating": true,
	"source": "console-api",
	"type": "log",
	"level": "log",
	"messageText": null,
	"parameters": [
		{
			"type": "undefined"
		}
	],
	"repeat": 1,
	"repeatId": "{\"id\":null,\"allowRepeating\":true,\"source\":\"console-api\",\"type\":\"log\",\"level\":\"log\",\"messageText\":null,\"parameters\":[{\"type\":\"undefined\"}],\"repeatId\":null,\"stacktrace\":null,\"frame\":{\"source\":\"http://example.com/browser/devtools/client/webconsole/new-console-output/test/fixtures/stub-generators/test-tempfile.js\",\"line\":1,\"column\":27}}",
	"stacktrace": null,
	"frame": {
		"source": "http://example.com/browser/devtools/client/webconsole/new-console-output/test/fixtures/stub-generators/test-tempfile.js",
		"line": 1,
		"column": 27
	}
}));

stubConsoleMessages.set("console.warn('danger, will robinson!')", new ConsoleMessage({
	"id": "1",
	"allowRepeating": true,
	"source": "console-api",
	"type": "warn",
	"level": "warn",
	"messageText": null,
	"parameters": [
		"danger, will robinson!"
	],
	"repeat": 1,
	"repeatId": "{\"id\":null,\"allowRepeating\":true,\"source\":\"console-api\",\"type\":\"warn\",\"level\":\"warn\",\"messageText\":null,\"parameters\":[\"danger, will robinson!\"],\"repeatId\":null,\"stacktrace\":null,\"frame\":{\"source\":\"http://example.com/browser/devtools/client/webconsole/new-console-output/test/fixtures/stub-generators/test-tempfile.js\",\"line\":1,\"column\":27}}",
	"stacktrace": null,
	"frame": {
		"source": "http://example.com/browser/devtools/client/webconsole/new-console-output/test/fixtures/stub-generators/test-tempfile.js",
		"line": 1,
		"column": 27
	}
}));

stubConsoleMessages.set("console.log(NaN)", new ConsoleMessage({
	"id": "1",
	"allowRepeating": true,
	"source": "console-api",
	"type": "log",
	"level": "log",
	"messageText": null,
	"parameters": [
		{
			"type": "NaN"
		}
	],
	"repeat": 1,
	"repeatId": "{\"id\":null,\"allowRepeating\":true,\"source\":\"console-api\",\"type\":\"log\",\"level\":\"log\",\"messageText\":null,\"parameters\":[{\"type\":\"NaN\"}],\"repeatId\":null,\"stacktrace\":null,\"frame\":{\"source\":\"http://example.com/browser/devtools/client/webconsole/new-console-output/test/fixtures/stub-generators/test-tempfile.js\",\"line\":1,\"column\":27}}",
	"stacktrace": null,
	"frame": {
		"source": "http://example.com/browser/devtools/client/webconsole/new-console-output/test/fixtures/stub-generators/test-tempfile.js",
		"line": 1,
		"column": 27
	}
}));

stubConsoleMessages.set("console.log(null)", new ConsoleMessage({
	"id": "1",
	"allowRepeating": true,
	"source": "console-api",
	"type": "log",
	"level": "log",
	"messageText": null,
	"parameters": [
		{
			"type": "null"
		}
	],
	"repeat": 1,
	"repeatId": "{\"id\":null,\"allowRepeating\":true,\"source\":\"console-api\",\"type\":\"log\",\"level\":\"log\",\"messageText\":null,\"parameters\":[{\"type\":\"null\"}],\"repeatId\":null,\"stacktrace\":null,\"frame\":{\"source\":\"http://example.com/browser/devtools/client/webconsole/new-console-output/test/fixtures/stub-generators/test-tempfile.js\",\"line\":1,\"column\":27}}",
	"stacktrace": null,
	"frame": {
		"source": "http://example.com/browser/devtools/client/webconsole/new-console-output/test/fixtures/stub-generators/test-tempfile.js",
		"line": 1,
		"column": 27
	}
}));

stubConsoleMessages.set("console.clear()", new ConsoleMessage({
	"id": "1",
	"allowRepeating": true,
	"source": "console-api",
	"type": "clear",
	"level": "log",
	"messageText": null,
	"parameters": [
		"Console was cleared."
	],
	"repeat": 1,
	"repeatId": "{\"id\":null,\"allowRepeating\":true,\"source\":\"console-api\",\"type\":\"clear\",\"level\":\"log\",\"messageText\":null,\"parameters\":[\"Console was cleared.\"],\"repeatId\":null,\"stacktrace\":null,\"frame\":{\"source\":\"http://example.com/browser/devtools/client/webconsole/new-console-output/test/fixtures/stub-generators/test-tempfile.js\",\"line\":1,\"column\":27}}",
	"stacktrace": null,
	"frame": {
		"source": "http://example.com/browser/devtools/client/webconsole/new-console-output/test/fixtures/stub-generators/test-tempfile.js",
		"line": 1,
		"column": 27
	}
}));

stubConsoleMessages.set("console.count('bar')", new ConsoleMessage({
	"id": "1",
	"allowRepeating": true,
	"source": "console-api",
	"type": "log",
	"level": "debug",
	"messageText": "bar: 1",
	"parameters": null,
	"repeat": 1,
	"repeatId": "{\"id\":null,\"allowRepeating\":true,\"source\":\"console-api\",\"type\":\"log\",\"level\":\"debug\",\"messageText\":\"bar: 1\",\"parameters\":null,\"repeatId\":null,\"stacktrace\":null,\"frame\":{\"source\":\"http://example.com/browser/devtools/client/webconsole/new-console-output/test/fixtures/stub-generators/test-tempfile.js\",\"line\":1,\"column\":27}}",
	"stacktrace": null,
	"frame": {
		"source": "http://example.com/browser/devtools/client/webconsole/new-console-output/test/fixtures/stub-generators/test-tempfile.js",
		"line": 1,
		"column": 27
	}
}));


// Temporarily hardcode these
stubConsoleMessages.set("new Date(0)", new ConsoleMessage({
	allowRepeating: true,
	source: MESSAGE_SOURCE.JAVASCRIPT,
	type: MESSAGE_TYPE.RESULT,
	level: MESSAGE_LEVEL.LOG,
	messageText: null,
	parameters: {
		"type": "object",
		"class": "Date",
		"actor": "server2.conn0.obj115",
		"extensible": true,
		"frozen": false,
		"sealed": false,
		"ownPropertyLength": 0,
		"preview": {
			"timestamp": 0
		}
	},
	repeat: 1,
	repeatId: null,
}));

stubConsoleMessages.set("ReferenceError", new ConsoleMessage({
	allowRepeating: true,
	source: MESSAGE_SOURCE.JAVASCRIPT,
	type: MESSAGE_TYPE.LOG,
	level: MESSAGE_LEVEL.ERROR,
	messageText: "ReferenceError: asdf is not defined",
	parameters: null,
	repeat: 1,
	repeatId: null,
}));

module.exports = {
  stubConsoleMessages
}