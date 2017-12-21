const Cu = Components.utils;
function run_test() {

  // Make a content sandbox with an Xrayable object.
  // NB: We use an nsEP here so that we can have access to Components, but still
  // have Xray behavior from this scope.
  var contentSB = new Cu.Sandbox(['http://www.google.com'],
                                 { wantGlobalProperties: ["XMLHttpRequest"], wantComponents: true });

  // Make an XHR in the content sandbox.
  Cu.evalInSandbox('xhr = new XMLHttpRequest();', contentSB);

  // Make sure that waivers can be set as Xray expandos.
  var xhr = contentSB.xhr;
  Assert.ok(Cu.isXrayWrapper(xhr));
  xhr.unwaivedExpando = xhr;
  Assert.ok(Cu.isXrayWrapper(xhr.unwaivedExpando));
  var waived = xhr.wrappedJSObject;
  Assert.ok(!Cu.isXrayWrapper(waived));
  xhr.waivedExpando = waived;
  Assert.ok(!Cu.isXrayWrapper(xhr.waivedExpando));

  // Try the same thing for getters/setters, even though that's kind of
  // contrived.
  Cu.evalInSandbox('function f() {}', contentSB);
  var f = contentSB.f;
  var fWaiver = Cu.waiveXrays(f);
  Assert.ok(f != fWaiver);
  Assert.ok(Cu.unwaiveXrays(fWaiver) === f);
  Object.defineProperty(xhr, 'waivedAccessors', {get: fWaiver, set: fWaiver});
  var desc = Object.getOwnPropertyDescriptor(xhr, 'waivedAccessors');
  Assert.ok(desc.get === fWaiver);
  Assert.ok(desc.set === fWaiver);

  // Make sure we correctly handle same-compartment security wrappers.
  var unwaivedC = contentSB.Components;
  Assert.ok(Cu.isXrayWrapper(unwaivedC));
  var waivedC = unwaivedC.wrappedJSObject;
  Assert.ok(waivedC && unwaivedC && (waivedC != unwaivedC));
  xhr.waivedC = waivedC;
  Assert.ok(xhr.waivedC === waivedC);
  Assert.ok(Cu.unwaiveXrays(xhr.waivedC) === unwaivedC);
}
