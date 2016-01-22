let { classes: Cc, utils: Cu, interfaces: Ci } = Components;

let cs = Cc["@mozilla.org/cookiemanager;1"]
           .getService(Ci.nsICookieManager2);

addMessageListener("getCookieCountAndClear", () => {
  let count = 0;
  for (let list = cs.enumerator; list.hasMoreElements(); list.getNext())
    ++count;
  cs.removeAll();

  sendAsyncMessage("getCookieCountAndClear:return", { count });
});

cs.removeAll();
