/*
  Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/
*/
// Bug 484305 - Load workers as UTF-8.
postMessage({ encoding: "KOI8-R", text: "Привет" });
postMessage({ encoding: "UTF-8", text: "п÷я─п╦п╡п╣я┌" });
