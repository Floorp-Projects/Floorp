/**
 * These tests test nsBrowserGlue's nsIContentPermissionPrompt
 * implementation behaviour with various types of
 * nsIContentPermissionRequests.
 */

"use strict";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Integration.jsm", this);

XPCOMUtils.defineLazyServiceGetter(this, "ContentPermissionPrompt",
                                   "@mozilla.org/content-permission/prompt;1",
                                   "nsIContentPermissionPrompt");

/**
 * This is a partial implementation of nsIContentPermissionType.
 *
 * @param {string} type
 *        The string defining what type of permission is being requested.
 *        Example: "geo", "desktop-notification".
 * @return nsIContentPermissionType implementation.
 */
function MockContentPermissionType(type) {
  this.type = type;
}

MockContentPermissionType.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentPermissionType]),
  // We expose the wrappedJSObject so that we can be sure
  // in some of our tests that we're passing the right
  // nsIContentPermissionType around.
  wrappedJSObject: this,
};

/**
 * This is a partial implementation of nsIContentPermissionRequest.
 *
 * @param {Array<nsIContentPermissionType>} typesArray
 *        The types to assign to this nsIContentPermissionRequest,
 *        in order. You probably want to use MockContentPermissionType.
 * @return nsIContentPermissionRequest implementation.
 */
function MockContentPermissionRequest(typesArray) {
  this.types = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
  for (let type of typesArray) {
    this.types.appendElement(type);
  }
}

MockContentPermissionRequest.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentPermissionRequest]),
  // We expose the wrappedJSObject so that we can be sure
  // in some of our tests that we're passing the right
  // nsIContentPermissionRequest around.
  wrappedJSObject: this,
  // For some of our tests, we want to make sure that the
  // request is cancelled, so we add some instrumentation here
  // to check that cancel() is called.
  cancel() {
    this.cancelled = true;
  },
  cancelled: false,
};

/**
 * Tests that if the nsIContentPermissionRequest has an empty
 * types array, that NS_ERROR_UNEXPECTED is thrown, and the
 * request is cancelled.
 */
add_task(function* test_empty_types() {
  let mockRequest = new MockContentPermissionRequest([]);
  Assert.throws(() => { ContentPermissionPrompt.prompt(mockRequest); },
                /NS_ERROR_UNEXPECTED/,
                "Should have thrown NS_ERROR_UNEXPECTED.");
  Assert.ok(mockRequest.cancelled, "Should have cancelled the request.");
});

/**
 * Tests that if the nsIContentPermissionRequest has more than
 * one type, that NS_ERROR_UNEXPECTED is thrown, and the request
 * is cancelled.
 */
add_task(function* test_multiple_types() {
  let mockRequest = new MockContentPermissionRequest([
    new MockContentPermissionType("test1"),
    new MockContentPermissionType("test2"),
  ]);

  Assert.throws(() => { ContentPermissionPrompt.prompt(mockRequest); },
                /NS_ERROR_UNEXPECTED/);
  Assert.ok(mockRequest.cancelled, "Should have cancelled the request.");
});

/**
 * Tests that if the nsIContentPermissionRequest has a type that
 * does not implement nsIContentPermissionType that NS_NOINTERFACE
 * is thrown, and the request is cancelled.
 */
add_task(function* test_not_permission_type() {
  let mockRequest = new MockContentPermissionRequest([
    { QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports]) },
  ]);

  Assert.throws(() => { ContentPermissionPrompt.prompt(mockRequest); },
                /NS_NOINTERFACE/);
  Assert.ok(mockRequest.cancelled, "Should have cancelled the request.");
});

/**
 * Tests that if the nsIContentPermissionRequest is for a type
 * that is not recognized, that NS_ERROR_FAILURE is thrown and
 * the request is cancelled.
 */
add_task(function* test_unrecognized_type() {
  let mockRequest = new MockContentPermissionRequest([
    new MockContentPermissionType("test1"),
  ]);

  Assert.throws(() => { ContentPermissionPrompt.prompt(mockRequest); },
                /NS_ERROR_FAILURE/);
  Assert.ok(mockRequest.cancelled, "Should have cancelled the request.");
});

/**
 * Tests that if we meet the minimal requirements for a
 * nsIContentPermissionRequest, that it will be passed to
 * ContentPermissionIntegration's createPermissionPrompt
 * method.
 */
add_task(function* test_working_request() {
  let mockType = new MockContentPermissionType("test-permission-type");
  let mockRequest = new MockContentPermissionRequest([mockType]);

  // mockPermissionPrompt is what createPermissionPrompt
  // will return. Returning some kind of object should be
  // enough to convince nsBrowserGlue that everything went
  // okay.
  let didPrompt = false;
  let mockPermissionPrompt = {
    prompt() {
      didPrompt = true;
    }
  };

  let integration = (base) => ({
    createPermissionPrompt(type, request) {
      Assert.equal(type, "test-permission-type");
      Assert.ok(Object.is(request.wrappedJSObject, mockRequest.wrappedJSObject));
      return mockPermissionPrompt;
    },
  });

  // Register an integration so that we can capture the
  // calls into ContentPermissionIntegration.
  try {
    Integration.contentPermission.register(integration);

    ContentPermissionPrompt.prompt(mockRequest);
    Assert.ok(!mockRequest.cancelled,
              "Should not have cancelled the request.");
    Assert.ok(didPrompt, "Should have tried to show the prompt");
  } finally {
    Integration.contentPermission.unregister(integration);
  }
});
