add_task(async function test_reserved_shortcuts() {
  /* eslint-disable no-unsanitized/property */
  let keyset = `<keyset>
                  <key id='kt_reserved' modifiers='shift' key='O' reserved='true' count='0'
                       oncommand='this.setAttribute("count", Number(this.getAttribute("count")) + 1)'/>
                  <key id='kt_notreserved' modifiers='shift' key='P' reserved='false' count='0'
                       oncommand='this.setAttribute("count", Number(this.getAttribute("count")) + 1)'/>
                  <key id='kt_reserveddefault' modifiers='shift' key='Q' count='0'
                       oncommand='this.setAttribute("count", Number(this.getAttribute("count")) + 1)'/>
                </keyset>`;

  let container = document.createElement("box");
  container.innerHTML = keyset;
  document.documentElement.appendChild(container);
  /* eslint-enable no-unsanitized/property */

  const pageUrl = "data:text/html,<body onload='document.body.firstChild.focus();'><div onkeydown='event.preventDefault();' tabindex=0>Test</div></body>";
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, pageUrl);

  EventUtils.synthesizeKey("O", { shiftKey: true });
  EventUtils.synthesizeKey("P", { shiftKey: true });
  EventUtils.synthesizeKey("Q", { shiftKey: true });

  is(document.getElementById("kt_reserved").getAttribute("count"), "1", "reserved='true' with preference off");
  is(document.getElementById("kt_notreserved").getAttribute("count"), "0", "reserved='false' with preference off");
  is(document.getElementById("kt_reserveddefault").getAttribute("count"), "0", "default reserved with preference off");

  // Now try with reserved shortcut key handling enabled.
  await new Promise(resolve => {
    SpecialPowers.pushPrefEnv({"set": [["permissions.default.shortcuts", 2]]}, resolve);
  });

  EventUtils.synthesizeKey("O", { shiftKey: true });
  EventUtils.synthesizeKey("P", { shiftKey: true });
  EventUtils.synthesizeKey("Q", { shiftKey: true });

  is(document.getElementById("kt_reserved").getAttribute("count"), "2", "reserved='true' with preference on");
  is(document.getElementById("kt_notreserved").getAttribute("count"), "0", "reserved='false' with preference on");
  is(document.getElementById("kt_reserveddefault").getAttribute("count"), "1", "default reserved with preference on");

  document.documentElement.removeChild(container);

  await BrowserTestUtils.removeTab(tab);
});
