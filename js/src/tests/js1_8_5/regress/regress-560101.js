// |reftest| pref(javascript.options.xml.content,true)
try {
    Object.defineProperty(<x/>, "p", {});  // don't assert
} catch (exc) {}
reportCompare(0, 0, "ok");