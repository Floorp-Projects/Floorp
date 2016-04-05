// Tests the filter search box in the storage inspector
"use strict";

add_task(function* () {
  yield openTabAndSetupStorage(MAIN_DOMAIN + "storage-search.html");

  let $$ = sel => gPanelWindow.document.querySelectorAll(sel);
  gUI.tree.expandAll();
  yield selectTreeItem(["localStorage", "http://test1.example.org"]);

  // Results: 0=hidden, 1=visible
  let testcases = [
    // Test that search isn't case-sensitive
    {
      value: "FoO",
      results: [0, 0, 1, 1, 0, 1, 0]
    },
    {
      value: "OR",
      results: [0, 1, 0, 0, 0, 1, 0]
    },
    {
      value: "aNImAl",
      results: [0, 1, 0, 0, 0, 0, 0]
    },
    // Test numbers
    {
      value: "01",
      results: [1, 0, 0, 0, 0, 0, 1]
    },
    {
      value: "2016",
      results: [0, 0, 0, 0, 0, 0, 1]
    },
    {
      value: "56789",
      results: [1, 0, 0, 0, 0, 0, 0]
    },
    // Test filtering by value
    {
      value: "horse",
      results: [0, 1, 0, 0, 0, 0, 0]
    },
    {
      value: "$$$",
      results: [0, 0, 0, 0, 1, 0, 0]
    },
    {
      value: "bar",
      results: [0, 0, 1, 1, 0, 0, 0]
    },
    // Test input with whitespace
    {
      value: "energy b",
      results: [0, 0, 0, 1, 0, 0, 0]
    },
    // Test no input at all
    {
      value: "",
      results: [1, 1, 1, 1, 1, 1, 1]
    },
    // Test input that matches nothing
    {
      value: "input that matches nothing",
      results: [0, 0, 0, 0, 0, 0, 0]
    }
  ];

  let names = $$("#name .table-widget-cell");
  let rows = $$("#value .table-widget-cell");
  for (let testcase of testcases) {
    info(`Testing input: ${testcase.value}`);

    gUI.searchBox.value = testcase.value;
    gUI.filterItems();

    for (let i = 0; i < rows.length; i++) {
      info(`Testing row ${i}`);
      info(`key: ${names[i].value}, value: ${rows[i].value}`);
      let state = testcase.results[i] ? "visible" : "hidden";
      is(rows[i].hasAttribute("hidden"), !testcase.results[i],
         `Row ${i} should be ${state}`);
    }
  }

  yield finishTests();
});
