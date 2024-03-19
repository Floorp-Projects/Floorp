/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

add_task(async function () {
  let tab1 = await addTab();
  let tab2 = await addTab();
  let tab3 = await addTab();
  let tabs = [tab1, tab2, tab3];

  // Add a beforeunload event listener in one of the tabs; it should be called
  // before closing any of the tabs.
  await ContentTask.spawn(tab2.linkedBrowser, null, async function () {
    content.window.addEventListener("beforeunload", function () {}, true);
  });

  let permitUnloadSpy = sinon.spy(tab2.linkedBrowser, "asyncPermitUnload");
  let removeTabSpy = sinon.spy(gBrowser, "removeTab");

  gBrowser.removeTabs(tabs);

  Assert.ok(permitUnloadSpy.calledOnce, "permitUnload was called only once");
  Assert.equal(
    removeTabSpy.callCount,
    tabs.length,
    "removeTab was called for every tab"
  );
  Assert.ok(
    permitUnloadSpy.lastCall.calledBefore(removeTabSpy.firstCall),
    "permitUnload was called before for first removeTab call"
  );

  removeTabSpy.restore();
  permitUnloadSpy.restore();
});
