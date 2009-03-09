// defines for prompt - button position
const ACCEPT = 0;
const ACCEPT_FUZZ = 1;
const DECLINE = 2;

// set if there should be a delay before prompt is accepted
var DELAYED_PROMPT = 0;

var prompt_delay = timeout * 2;

// the prompt that was registered at runtime
var old_prompt;

// whether the prompt was accepted
var prompted = 0;

// which prompt option to select when prompt is fired
var promptOption;

// number of position changes for testLocationProvider to make
var num_pos_changes = 3;

 // based on testLocationProvider's interval
var timer_interval = 500;
var slack = 500;

// time needed for provider to make position changes
var timeout = num_pos_changes * timer_interval + slack;

function check_geolocation(location) {

  ok(location, "Check to see if this location is non-null");

  ok("timestamp" in location, "Check to see if there is a timestamp");

  // eventually, coords may be optional (eg, when civic addresses are supported)
  ok("coords" in location, "Check to see if this location has a coords");

  var coords = location.coords;

  ok("latitude" in coords, "Check to see if there is a latitude");
  ok("longitude" in coords, "Check to see if there is a longitude");
  ok("altitude" in coords, "Check to see if there is a altitude");
  ok("accuracy" in coords, "Check to see if there is a accuracy");
  ok("altitudeAccuracy" in coords, "Check to see if there is a alt accuracy");
  ok("heading" in coords, "Check to see if there is a heading");
  ok("speed" in coords, "Check to see if there is a speed");
}

//TODO: test for fuzzed location when this is implemented
function check_fuzzed_geolocation(location) {
  check_geolocation(location);
}

function check_no_geolocation(location) {
   ok(!location, "Check to see if this location is null");
}

function success_callback(position) {
  if(prompted == 0)
    ok(0, "Should not call success callback before prompt accepted");
  if(position == null)
    ok(1, "No geolocation available");
  else {
    switch(promptOption) {
      case ACCEPT:
        check_geolocation(position);
        break;
      case ACCEPT_FUZZ:
        check_fuzzed_geolocation(position);
        break;
      case DECLINE:
        check_no_geolocation(position);
        break;
      default:
        break;
    }
  }
}

function geolocation_prompt(request) {
  netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');
	prompted = 1;
  switch(promptOption) {
    case ACCEPT:
      request.allow();
      break;
    case ACCEPT_FUZZ:
      request.allowButFuzz();
      break;
    case DECLINE:
      request.cancel();
      break;
    default:
      break;
  }
  return 1;
}

function delayed_prompt(request) {
  setTimeout(geolocation_prompt, prompt_delay, request);
}

var TestPromptFactory = {
    QueryInterface: function(iid) {
        if (iid.equals(Components.interfaces.nsISupports) || iid.equals(Components.interfaces.nsIFactory))
            return this;
        throw Components.results.NS_ERROR_NO_INTERFACE;
    },

    createInstance: function(outer, iid) {
        if (outer)
            throw Components.results.NS_ERROR_NO_AGGREGATION;

        if(DELAYED_PROMPT)
            return delayed_prompt;
        else
            return  geolocation_prompt;
    },

    lockFactory: function(lock) {
        throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    },
};

function attachPrompt() {
  netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');
  old_prompt  =  Components.manager.nsIComponentRegistrar.contractIDToCID("@mozilla.org/geolocation/prompt;1");
  old_factory =  Components.manager.getClassObjectByContractID("@mozilla.org/geolocation/prompt;1", Components.interfaces.nsIFactory)

  const testing_prompt_cid = Components.ID("{20C27ECF-A22E-4022-9757-2CFDA88EAEAA}");
  Components.manager.nsIComponentRegistrar.registerFactory(testing_prompt_cid,
                                                           "Test Geolocation Prompt",
                                                           "@mozilla.org/geolocation/prompt;1",
                                                           TestPromptFactory);
}

function removePrompt() {
  netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');
  const testing_prompt_cid = Components.ID("{20C27ECF-A22E-4022-9757-2CFDA88EAEAA}");
  Components.manager.nsIComponentRegistrar.unregisterFactory(testing_prompt_cid, TestPromptFactory);
  Components.manager.nsIComponentRegistrar.registerFactory(old_prompt,
                                                           "Geolocation Prompt restored!",
                                                           "@mozilla.org/geolocation/prompt;1",
                                                           old_factory);
}
