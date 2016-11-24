/**
 * Cleans up the testing environment.
 */

/* global resHandler */

"use strict";

// Unregister the resource path of formautofill.
resHandler.setSubstitution("formautofill", null);
