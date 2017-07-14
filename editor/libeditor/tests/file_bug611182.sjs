// SJS file for test_bug611182.html
"use strict";

const TESTS = [
  {
    ct: "text/html",
    val: "<html contenteditable>fooz bar</html>",
  },
  {
    ct: "text/html",
    val: "<html contenteditable><body>fooz bar</body></html>",
  },
  {
    ct: "text/html",
    val: "<body contenteditable>fooz bar</body>",
  },
  {
    ct: "text/html",
    val: "<body contenteditable><p>fooz bar</p></body>",
  },
  {
    ct: "text/html",
    val: "<body contenteditable><div>fooz bar</div></body>",
  },
  {
    ct: "text/html",
    val: "<body contenteditable><span>fooz bar</span></body>",
  },
  {
    ct: "text/html",
    val: "<p contenteditable style='outline:none'>fooz bar</p>",
  },
  {
    ct: "text/html",
    val: "<!DOCTYPE html><html><body contenteditable>fooz bar</body></html>",
  },
  {
    ct: "text/html",
    val: "<!DOCTYPE html><html contenteditable><body>fooz bar</body></html>",
  },
  {
    ct: "application/xhtml+xml",
    val: '<html xmlns="http://www.w3.org/1999/xhtml"><body contenteditable="true">fooz bar</body></html>',
  },
  {
    ct: "application/xhtml+xml",
    val: '<html xmlns="http://www.w3.org/1999/xhtml" contenteditable="true"><body>fooz bar</body></html>',
  },
  {
    ct: "text/html",
    val: "<body onload=\"document.designMode='on'\">fooz bar</body>",
  },
  {
    ct: "text/html",
    val: '<html><script>' +
          'onload = function() {' +
            'var old = document.body;' +
            'old.parentNode.removeChild(old);' +
            'var r = document.documentElement;' +
            'var b = document.createElement("body");' +
            'r.appendChild(b);' +
            'b.appendChild(document.createTextNode("fooz bar"));' +
            'b.contentEditable = "true";' +
         '};' +
        '<\/script><body></body></html>',
  },
  {
    ct: "text/html",
    val: '<html><script>' +
           'onload = function() {' +
             'var old = document.body;' +
             'old.parentNode.removeChild(old);' +
             'var r = document.documentElement;' +
             'var b = document.createElement("body");' +
             'b.appendChild(document.createTextNode("fooz bar"));' +
             'b.contentEditable = "true";' +
             'r.appendChild(b);' +
           '};' +
         '<\/script><body></body></html>',
  },
  {
    ct: "text/html",
    val: '<html><script>' +
          'onload = function() {' +
            'var old = document.body;' +
            'old.parentNode.removeChild(old);' +
            'var r = document.documentElement;' +
            'var b = document.createElement("body");' +
            'r.appendChild(b);' +
            'b.appendChild(document.createTextNode("fooz bar"));' +
            'b.setAttribute("contenteditable", "true");' +
          '};' +
         '<\/script><body></body></html>',
  },
  {
    ct: "text/html",
    val: '<html><script>' +
          'onload = function() {' +
            'var old = document.body;' +
            'old.parentNode.removeChild(old);' +
            'var r = document.documentElement;' +
            'var b = document.createElement("body");' +
            'b.appendChild(document.createTextNode("fooz bar"));' +
            'b.setAttribute("contenteditable", "true");' +
            'r.appendChild(b);' +
          '};' +
         '<\/script><body></body></html>',
  },
  {
    ct: "text/html",
    val: '<html><script>' +
           'onload = function() {' +
             'var old = document.body;' +
             'old.parentNode.removeChild(old);' +
             'var r = document.documentElement;' +
             'var b = document.createElement("body");' +
             'r.appendChild(b);' +
             'b.contentEditable = "true";' +
             'b.appendChild(document.createTextNode("fooz bar"));' +
           '};' +
          '<\/script><body></body></html>',
  },
  {
    ct: "text/html",
    val: '<html><script>' +
          'onload = function() {' +
            'var old = document.body;' +
            'old.parentNode.removeChild(old);' +
            'var r = document.documentElement;' +
            'var b = document.createElement("body");' +
            'b.contentEditable = "true";' +
            'r.appendChild(b);' +
            'b.appendChild(document.createTextNode("fooz bar"));' +
          '};' +
         '<\/script><body></body></html>',
  },
  {
    ct: "text/html",
    val: '<html><script>' +
          'onload = function() {' +
            'var old = document.body;' +
            'old.parentNode.removeChild(old);' +
            'var r = document.documentElement;' +
            'var b = document.createElement("body");' +
            'r.appendChild(b);' +
            'b.setAttribute("contenteditable", "true");' +
            'b.appendChild(document.createTextNode("fooz bar"));' +
          '};' +
         '<\/script><body></body></html>',
  },
  {
    ct: "text/html",
    val: '<html><script>' +
          'onload = function() {' +
            'var old = document.body;' +
            'old.parentNode.removeChild(old);' +
            'var r = document.documentElement;' +
            'var b = document.createElement("body");' +
            'b.setAttribute("contenteditable", "true");' +
            'r.appendChild(b);' +
            'b.appendChild(document.createTextNode("fooz bar"));' +
          '};' +
         '<\/script><body></body></html>',
  },
  {
    ct: "text/html",
    val: '<html><script>' +
          'onload = function() {' +
            'document.open();' +
            'document.write("<body contenteditable>fooz bar</body>");' +
            'document.close();' +
          '};' +
         '<\/script><body></body></html>',
  },
  {
    ct: "text/html",
    val: 'data:text/html,<html><script>' +
          'onload = function() {' +
            'document.open();' +
            'document.write("<body contenteditable><div>fooz bar</div></body>");' +
            'document.close();' +
          '};' +
         '<\/script><body></body></html>',
  },
  {
    ct: "text/html",
    val: '<html><script>' +
          'onload = function() {' +
            'document.open();' +
            'document.write("<body contenteditable><span>fooz bar</span></body>");' +
            'document.close();' +
          '};' +
         '<\/script><body></body></html>',
  },
  {
    ct: "text/html",
    val: '<html><script>' +
          'onload = function() {' +
            'document.open();' +
            'document.write("<p contenteditable style=\\"outline: none\\">fooz bar</p>");' +
            'document.close();' +
           '};' +
         '<\/script><body></body></html>',
  },
  {
    ct: "text/html",
    val: '<html><script>' +
          'onload = function() {' +
            'document.open();' +
            'document.write("<html contenteditable>fooz bar</html>");' +
            'document.close();' +
          '};' +
         '<\/script><body></body></html>',
  },
  {
    ct: "text/html",
    val: '<html><script>' +
          'onload = function() {' +
            'document.open();' +
            'document.write("<html contenteditable><body>fooz bar</body></html>");' +
            'document.close();' +
          '};' +
         '<\/script><body></body></html>',
  },
];

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);

  let query = request.queryString;
  if (query === "queryTotalTests") {
    response.setHeader("Content-Type", "text/html", false);
    response.write(TESTS.length);
    return;
  }

  var curTest = TESTS[query];
  response.setHeader("Content-Type", curTest.ct, false);
  response.write(curTest.val);
}
