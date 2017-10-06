/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function handleRequest(request, response)
{
  var page = "<!DOCTYPE html><html>" +
    "<head><meta charset='utf-8'></head>" +
    "<body><p>hello world!</p></body>" +
    "</html>";

  var data = {
    "version": "4.1.0",
    "columns": ["log", "backtrace", "type"],
    "rows":[[
      [{ "best": "Firefox", "reckless": "Chrome", "new_ie": "Safari", "new_new_ie": "Edge"}],
      "C:\\src\\www\\serverlogging\\test7.php:4:1",
      ""
    ]],
  };

  // Put log into headers.
  var value = b64EncodeUnicode(JSON.stringify(data));
  response.setHeader("X-ChromeLogger-Data", value, false);

  response.write(page);
}

function b64EncodeUnicode(str) {
  return btoa(unescape(encodeURIComponent(str)));
}
