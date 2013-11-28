function test() {
  is(document.querySelectorAll("#appmenu-popup [accesskey]").length, 0,
     "there should be no items with access keys in the app menu popup");
}
