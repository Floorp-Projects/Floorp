function test(){
  var tab = gBrowser.addTab();
  ok(tab.getAttribute("closetabtext") != "", "tab has non-empty closetabtext");
  gBrowser.removeTab(tab);  
}
