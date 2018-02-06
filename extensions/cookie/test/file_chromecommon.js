let cs = Cc["@mozilla.org/cookiemanager;1"]
           .getService(Ci.nsICookieManager);

addMessageListener("getCookieCountAndClear", () => {
  let count = 0;
  for (let list = cs.enumerator; list.hasMoreElements(); list.getNext())
    ++count;
  cs.removeAll();

  sendAsyncMessage("getCookieCountAndClear:return", { count });
});

cs.removeAll();
