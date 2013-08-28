// screen.js:
// Set the screen size, pixel density and scaling of the b2g client screen
// based on the --screen command-line option, if there is one.
// 
// TODO: support multiple device pixels per CSS pixel
// 

// We do this on ContentStart because querying the displayDPI fails otherwise.
window.addEventListener('ContentStart', function() {
  // This is the toplevel <window> element
  let shell = document.getElementById('shell');

  // The <browser> element inside it
  let browser = document.getElementById('systemapp');

  // Figure out the native resolution of the screen
  let windowUtils = window.QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Components.interfaces.nsIDOMWindowUtils);
  let hostDPI = windowUtils.displayDPI;

  let DEFAULT_SCREEN = "320x480";

  // This is a somewhat random selection of named screens.
  // Add more to this list when we support more hardware.
  // Data from: http://en.wikipedia.org/wiki/List_of_displays_by_pixel_density
  let screens = {
    iphone: {
      name: 'Apple iPhone', width:320, height:480,  dpi:163
    },
    ipad: {
      name: 'Apple iPad', width:1024, height:768,  dpi:132
    },
    nexus_s: {
      name: 'Samsung Nexus S', width:480, height:800, dpi:235
    },
    galaxy_s2: {
      name: 'Samsung Galaxy SII (I9100)', width:480, height:800, dpi:219
    },
    galaxy_nexus: {
      name: 'Samsung Galaxy Nexus', width:720, height:1280, dpi:316
    },
    galaxy_tab: {
      name: 'Samsung Galaxy Tab 10.1', width:800, height:1280, dpi:149
    },
    wildfire: {
      name: 'HTC Wildfire', width:240, height:320, dpi:125
    },
    tattoo: {
      name: 'HTC Tattoo', width:240, height:320, dpi:143
    },
    salsa: {
      name: 'HTC Salsa', width:320, height:480, dpi:170
    },
    chacha: {
      name: 'HTC ChaCha', width:320, height:480, dpi:222
    },
  };

  // Get the command line arguments that were passed to the b2g client
  let args = window.arguments[0].QueryInterface(Ci.nsICommandLine);
  let screenarg;

  // Get the --screen argument from the command line
  try {
    screenarg = args.handleFlagWithParam('screen', false);

    // If there isn't one, use the default screen
    if (screenarg === null)
      screenarg = DEFAULT_SCREEN;

    // With no value, tell the user how to use it
    if (screenarg == '')
      usage();
  }
  catch(e) {
    // If getting the argument value fails, its an error
    usage();
  }
  
  // Special case --screen=full goes into fullscreen mode
  if (screenarg === 'full') {
    shell.setAttribute('sizemode', 'fullscreen');
    return;
  } 

  let rescale = false;

  // If the value of --screen ends with !, we'll be scaling the output
  if (screenarg[screenarg.length - 1] === '!') {
    rescale = true;
    screenarg = screenarg.substring(0, screenarg.length-1);
  }

  let width, height, dpi;

  if (screenarg in screens) {
    // If this is a named screen, get its data
    let screen = screens[screenarg];
    width = screen.width;
    height = screen.height;
    dpi = screen.dpi;
  } else {
    // Otherwise, parse the resolution and density from the --screen value.
    // The supported syntax is WIDTHxHEIGHT[@DPI]
    let match = screenarg.match(/^(\d+)x(\d+)(@(\d+))?$/);
    
    // Display usage information on syntax errors
    if (match == null)
      usage();
    
    // Convert strings to integers
    width = parseInt(match[1], 10);
    height = parseInt(match[2], 10);
    if (match[4])
      dpi = parseInt(match[4], 10);
    else    // If no DPI, use the actual dpi of the host screen
      dpi = hostDPI;

    // If any of the values came out 0 or NaN or undefined, display usage
    if (!width || !height || !dpi)
      usage();
  }
  
  // In order to do rescaling, we set the <browser> tag to the specified
  // width and height, and then use a CSS transform to scale it so that
  // it appears at the correct size on the host display.  We also set
  // the size of the <window> element to that scaled target size.
  let scale = rescale ? hostDPI / dpi : 1;

  // Set the window width and height to desired size plus chrome
  let chromewidth = window.outerWidth - window.innerWidth;
  let chromeheight = window.outerHeight - window.innerHeight;
  window.resizeTo(Math.round(width * scale) + chromewidth,
                  Math.round(height * scale) + chromeheight);

  // Set the browser element to the full unscaled size of the screen
  browser.style.width = browser.style.minWidth = browser.style.maxWidth =
    width + 'px';
  browser.style.height = browser.style.minHeight = browser.style.maxHeight =
    height + 'px';
  browser.setAttribute('flex', '0');  // Don't let it stretch

  // Now scale the browser element as needed
  if (scale !== 1) {
    browser.style.MozTransformOrigin = 'top left';
    browser.style.MozTransform = 'scale(' + scale + ',' + scale + ')';
  }

  // Set the pixel density that we want to simulate.
  // This doesn't change the on-screen size, but makes
  // CSS media queries and mozmm units work right.
  Services.prefs.setIntPref('layout.css.dpi', dpi);

  // A utility function like console.log() for printing to the terminal window
  // Uses dump(), but enables it first, if necessary
  function print() {
    let dump_enabled =
      Services.prefs.getBoolPref('browser.dom.window.dump.enabled');

    if (!dump_enabled) 
      Services.prefs.setBoolPref('browser.dom.window.dump.enabled', true);

    dump(Array.prototype.join.call(arguments, ' ') + '\n');

    if (!dump_enabled) 
      Services.prefs.setBoolPref('browser.dom.window.dump.enabled', false);
  }

  // Print usage info for --screen and exit
  function usage() {
    // Documentation for the --screen argument
    let msg = 
      'The --screen argument specifies the desired resolution and\n' +
      'pixel density of the simulated device screen. Use it like this:\n' +
      '\t--screen=WIDTHxHEIGHT\t\t\t// E.g.: --screen=320x480\n' +
      '\t--screen=WIDTHxHEIGHT@DOTS_PER_INCH\t// E.g.: --screen=480x800@250\n' +
      '\t--screen=full\t\t\t\t// run in fullscreen mode\n' +
      '\nYou can also specify certain device names:\n';
    for(let p in screens)
      msg += '\t--screen=' + p + '\t// ' + screens[p].name + '\n';
    msg += 
      '\nAdd a ! to the end of a screen specification to rescale the\n' +
      'screen so that it is shown at actual size on your monitor:\n' +
      '\t--screen=nexus_s!\n' +
      '\t--screen=320x480@200!\n'
    ;

    // Display the usage message
    print(msg);

    // Exit the b2g client
    Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit);
  }
});
