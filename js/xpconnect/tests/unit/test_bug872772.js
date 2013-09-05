const Cu = Components.utils;
function run_test() {

  // Make a content sandbox with an Xrayable object.
  // NB: We use an nsEP here so that we can have access to Components, but still
  // have Xray behavior from this scope.
  var contentSB = new Cu.Sandbox(['http://www.google.com'],
                                 { wantDOMConstructors: ["XMLHttpRequest"], wantComponents: true });

  // Make an XHR in the content sandbox.
  Cu.evalInSandbox('xhr = new XMLHttpRequest();', contentSB);

  // Make sure that waivers can be set as Xray expandos.
  var xhr = contentSB.xhr;
  do_check_true(Cu.isXrayWrapper(xhr));
  xhr.unwaivedExpando = xhr;
  do_check_true(Cu.isXrayWrapper(xhr.unwaivedExpando));
  var waived = xhr.wrappedJSObject;
  do_check_true(!Cu.isXrayWrapper(waived));
  xhr.waivedExpando = waived;
  do_check_true(!Cu.isXrayWrapper(xhr.waivedExpando));

  // Try the same thing for getters/setters, even though that's kind of
  // contrived.
  Cu.evalInSandbox('function f() {}', contentSB);
  var f = contentSB.f;
  var fWaiver = Cu.waiveXrays(f);
  do_check_true(f != fWaiver);
  do_check_true(Cu.unwaiveXrays(fWaiver) === f);
  Object.defineProperty(xhr, 'waivedAccessors', {get: fWaiver, set: fWaiver});
  var desc = Object.getOwnPropertyDescriptor(xhr, 'waivedAccessors');
  do_check_true(desc.get === fWaiver);
  do_check_true(desc.set === fWaiver);

  // Make sure we correctly handle same-compartment security wrappers.
  var unwaivedC = contentSB.Components;
  do_check_true(Cu.isXrayWrapper(unwaivedC));
  var waivedC = unwaivedC.wrappedJSObject;
  do_check_true(waivedC && unwaivedC && (waivedC != unwaivedC));
  xhr.waivedC = waivedC;
  do_check_true(xhr.waivedC === waivedC);
  do_check_true(Cu.unwaiveXrays(xhr.waivedC) === unwaivedC);
}
