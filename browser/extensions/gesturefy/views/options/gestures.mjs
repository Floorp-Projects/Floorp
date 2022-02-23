import { ContentLoaded, Config } from "/views/options/main.mjs";

import MouseGestureController from "/core/controllers/mouse-gesture-controller.mjs";

import Gesture from "/core/models/gesture.mjs";

import PatternConstructor from "/core/utils/pattern-constructor.mjs";

import { getClosestGestureByPattern } from "/core/utils/matching-algorithms.mjs";

ContentLoaded.then(main);

// reference to the curently active gesture list item
let currentItem = null;

// stores the current pattern of the gesture popup
let currentPopupPattern = null;

const Gestures = new Map();


/**
 * main function
 * run code that depends on async resources
 **/
function main (values) {
  // add event listeners
  const gesturePopup = document.getElementById("gesturePopup");
        gesturePopup.onclose = onGesturePopupClose;
  const gesturePopupForm = document.getElementById("gesturePopupForm");
        gesturePopupForm.onsubmit = onGesturePopupFormSubmit;
  const gesturePopupCommandSelect = document.getElementById("gesturePopupCommandSelect")
        gesturePopupCommandSelect.onchange = onCommandSelectChange;
  const newGestureButton = document.getElementById("gestureAddButton");
        newGestureButton.onclick = onAddButtonClick;
  const gestureSearchToggleButton = document.getElementById("gestureSearchToggleButton");
        gestureSearchToggleButton.onclick = onSearchToggle;
  const gestureSearchInput = document.getElementById("gestureSearchInput");
        gestureSearchInput.oninput = onSearchInput;
        gestureSearchInput.placeholder = browser.i18n.getMessage('gestureSearchPlaceholder');
  // create and add all existing gesture items
  const fragment = document.createDocumentFragment();
  for (let gestureJSON of Config.get("Gestures")) {
    const gesture = new Gesture(gestureJSON);
    const gestureListItem = createGestureListItem(gesture);
    // use the reference to the gestureItem as the Map key to the gesture object
    Gestures.set(gestureListItem, gesture);
    fragment.prepend(gestureListItem);
  }
  const gestureList = document.getElementById("gestureContainer");
        gestureList.appendChild(fragment);
        gestureList.dataset.noResultsHint = browser.i18n.getMessage('gestureHintNoSearchResults');
  // add mouse gesture controller event listeners
  mouseGestureControllerSetup();
}


/**
 * Creates and returns a smooth svg path element from given points
 **/
function createCatmullRomSVGPath(points, alpha = 0.5) {
  let path = `M${points[0].x},${points[0].y} C`;

  const size = points.length - 1;

  for (let i = 0; i < size; i++) {
    const p0 = i === 0 ? points[0] : points[i - 1],
          p1 = points[i],
          p2 = points[i + 1],
          p3 = i === size - 1 ? p2 : points[i + 2];

    const d1 = Math.sqrt(Math.pow(p0.x - p1.x, 2) + Math.pow(p0.y - p1.y, 2)),
          d2 = Math.sqrt(Math.pow(p1.x - p2.x, 2) + Math.pow(p1.y - p2.y, 2)),
          d3 = Math.sqrt(Math.pow(p2.x - p3.x, 2) + Math.pow(p2.y - p3.y, 2));

    const d3powA  = Math.pow(d3, alpha),
          d3pow2A = Math.pow(d3, 2 * alpha),
          d2powA  = Math.pow(d2, alpha),
          d2pow2A = Math.pow(d2, 2 * alpha),
          d1powA  = Math.pow(d1, alpha),
          d1pow2A = Math.pow(d1, 2 * alpha);

    const A = 2 * d1pow2A + 3 * d1powA * d2powA + d2pow2A,
          B = 2 * d3pow2A + 3 * d3powA * d2powA + d2pow2A;

    let N = 3 * d1powA * (d1powA + d2powA),
        M = 3 * d3powA * (d3powA + d2powA);

    if (N > 0) N = 1 / N;
    if (M > 0) M = 1 / M;

    let x1 = (-d2pow2A * p0.x + A * p1.x + d1pow2A * p2.x) * N,
        y1 = (-d2pow2A * p0.y + A * p1.y + d1pow2A * p2.y) * N;

    let x2 = (d3pow2A * p1.x + B * p2.x - d2pow2A * p3.x) * M,
        y2 = (d3pow2A * p1.y + B * p2.y - d2pow2A * p3.y) * M;

    if (x1 === 0 && y1 === 0) {
      x1 = p1.x;
      y1 = p1.y;
    }

    if (x2 === 0 && y2 === 0) {
      x2 = p2.x;
      y2 = p2.y;
    }

    path += ` ${x1},${y1},${x2},${y2},${p2.x},${p2.y}`;
  }
  // create path element
  const pathElement = document.createElementNS('http://www.w3.org/2000/svg', 'path');
        pathElement.setAttribute("d", path);

  return pathElement;
}


/**
 * Creates and returns a svg element of a given gesture pattern
 **/
function createGestureThumbnail (pattern) {
  const viewBoxWidth = 100;
  const viewBoxHeight = 100;

  // convert vector array to points starting by 0, 0
  const points = [ {x: 0, y: 0} ];
  pattern.forEach((vector, i) => points.push({
    x: points[i].x + vector[0],
    y: points[i].y + vector[1]
  }));

  const svgElement = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
  // scales the svg elements to always fit the svg canvas
  svgElement.setAttribute("preserveAspectRatio", "xMidYMid meet");
  svgElement.setAttribute("viewBox", `${0} ${0} ${viewBoxWidth} ${viewBoxHeight}`);

  const group = document.createElementNS('http://www.w3.org/2000/svg', 'g');

  // create gesture trail as svg path element
  const gesturePathElement = createCatmullRomSVGPath(points);
  gesturePathElement.classList.add("gl-thumbnail-trail");

  // create arrow as svg path element
  const arrowPathElement = document.createElementNS('http://www.w3.org/2000/svg', 'path');
  arrowPathElement.setAttribute("d", `M0,-7 L14,0 L0,7 z`);
  arrowPathElement.classList.add("gl-thumbnail-arrow");
  arrowPathElement.style.setProperty("offset-path", `path('${gesturePathElement.getAttribute("d")}')`);

  group.append(gesturePathElement, arrowPathElement);

  svgElement.append(group);

  // append svg element hiddenly to dom in order to calculate necessary bounding boxes
  svgElement.style.cssText = "position: absolute; visibility: hidden;";
  document.body.appendChild(svgElement);
  const pathBBox = gesturePathElement.getBBox();
  document.body.removeChild(svgElement);
  svgElement.style.cssText = null;

  const scale = Math.min(viewBoxWidth/pathBBox.width, viewBoxHeight/pathBBox.height) * 0.75;

  // move path into view and scale it down
  let translateX = -pathBBox.x * scale;
  let translateY = -pathBBox.y * scale;

  // center path in the view
  translateX += viewBoxWidth/2 - pathBBox.width * scale / 2;
  translateY += viewBoxHeight/2 - pathBBox.height * scale / 2;

  group.style.setProperty("transform", `
    translate(${translateX}px, ${translateY}px)
    scale(var(--pathScale))
  `);

  // add path length and scale as css variables for animations and styling
  const gesturePathLength = gesturePathElement.getTotalLength();
  svgElement.style.setProperty("--pathLength", gesturePathLength);
  svgElement.style.setProperty("--pathScale", scale);

  return svgElement;
}


/**
 * Creates a gesture list item html element by a given gestureObject and returns it
 **/
function createGestureListItem (gesture) {
  const gestureListItem = document.createElement("li");
        gestureListItem.classList.add("gl-item");
        gestureListItem.onclick = onItemClick;
        gestureListItem.onpointerenter = onItemPointerenter;
        gestureListItem.onpointerleave = onItemPointerleave;
  const gestureThumbnail = createGestureThumbnail( gesture.getPattern() );
        gestureThumbnail.classList.add("gl-thumbnail");
  const commandField = document.createElement("div");
        commandField.classList.add("gl-command");
        commandField.textContent = gesture.toString();
  const removeButton = document.createElement("button");
        removeButton.classList.add("gl-remove-button", "icon-delete");
  gestureListItem.append(gestureThumbnail, commandField, removeButton);
  return gestureListItem;
}


/**
 * Adds a given gesture list item to the gesture list ui
 **/
function addGestureListItem (gestureListItem) {
  const gestureList = document.getElementById("gestureContainer");
  const gestureAddButtonItem = gestureList.firstElementChild;

  // prepend new entry, this pushes all elements by the height / width of one entry
  gestureAddButtonItem.after(gestureListItem);

  // select all visible gesture items
  const gestureItems = gestureList.querySelectorAll(".gl-item:not([hidden])");

  // check if at least one node already exists
  if (gestureItems.length > 0) {
    // get grid gaps and grid sizes
    const gridComputedStyle = window.getComputedStyle(gestureList);
    const gridRowGap = parseFloat(gridComputedStyle.getPropertyValue("grid-row-gap"));
    const gridColumnGap = parseFloat(gridComputedStyle.getPropertyValue("grid-column-gap"));
    const gridRowSizes = gridComputedStyle.getPropertyValue("grid-template-rows").split(" ").map(parseFloat);
    const gridColumnSizes = gridComputedStyle.getPropertyValue("grid-template-columns").split(" ").map(parseFloat);

    // translate all elements to their previous positions (minus the dimensions of one grid item)
    for (let i = 0; i < gestureItems.length; i++) {
      const gestureItem = gestureItems[i];
      // get corresponding grid row and column size
      const gridColumnSize = gridColumnSizes[i % gridColumnSizes.length];
      //const gridRowSize = gridRowSizes[Math.floor(i / gridColumnSizes.length)];
      // console.log("template rows", gridComputedStyle.getPropertyValue("grid-template-rows"));
      const gridRowSize = gridRowSizes[0];

      // translate last element of row one row up and to the right end
      if ((i + 1) % gridColumnSizes.length === 0) {
        gestureItem.style.setProperty('transform', `translate(
          ${(gridColumnSize + gridColumnGap) * (gridColumnSizes.length - 1)}px,
          ${-gridRowSize - gridRowGap}px)
        `);
      }
      else {
        gestureItem.style.setProperty('transform', `translateX(${-gridColumnSize - gridColumnGap}px)`);
      }
    }
  }

  // remove animation class on animation end
  gestureListItem.addEventListener('animationend', event => {
    event.currentTarget.classList.remove('gl-item-animate-add');

    // remove transform so all elements move to their new position
    for (const gestureItem of gestureItems) {
      gestureItem.addEventListener('transitionend', event => {
        event.currentTarget.style.removeProperty('transition')
      }, {once: true });
      gestureItem.style.setProperty('transition', 'transform .3s');
      gestureItem.style.removeProperty('transform');
    }
  }, {once: true });

  // setup gesture item add animation
  gestureListItem.classList.add('gl-item-animate-add');
  gestureListItem.style.transform += `scale(1.6)`;
  // trigger reflow / gesture item add animation
  gestureListItem.offsetHeight;
  gestureListItem.style.setProperty('transition', 'transform .3s');
  gestureListItem.style.transform = gestureListItem.style.transform.replace("scale(1.6)", "");
}


/**
 * Updates a given gesture list item ui by the provided gestureObject
 **/
function updateGestureListItem (gestureListItem, gesture) {
  // add popout animation for the updated gesture list item
  gestureListItem.addEventListener("animationend", () => {
    gestureListItem.classList.remove("gl-item-animate-update");
  }, { once: true });
  gestureListItem.classList.add("gl-item-animate-update");

  const currentGestureThumbnail = gestureListItem.querySelector(".gl-thumbnail");
  const newGestureThumbnail = createGestureThumbnail( gesture.getPattern() );
  newGestureThumbnail.classList.add("gl-thumbnail");
  currentGestureThumbnail.replaceWith(newGestureThumbnail);

  const commandField = gestureListItem.querySelector(".gl-command");
  commandField.textContent = gesture.toString();
}


/**
 * Removes a given gesture list item from the gesture list ui
 **/
function removeGestureListItem (gestureListItem) {
  const gestureList = document.getElementById("gestureContainer");
  // get child index for current gesture item
  const gestureItemIndex = Array.prototype.indexOf.call(gestureList.children, gestureListItem);
  // select all visible gesture items starting from given gesture item index
  const gestureItems = gestureList.querySelectorAll(`.gl-item:not([hidden]):nth-child(n + ${gestureItemIndex + 1})`);

  // for performance improvements: read and cache all grid item positions first
  const itemPositionCache = new Map();
  gestureItems.forEach(gestureItem => itemPositionCache.set(gestureItem, {x: gestureItem.offsetLeft, y: gestureItem.offsetTop}));

  // remove styles after transition
  function handleTransitionEnd (event) {
    event.currentTarget.style.removeProperty('transition');
    event.currentTarget.style.removeProperty('transform');
    event.currentTarget.removeEventListener('transitionend', handleTransitionEnd);
  }

  // remove element on animation end
  function handleAnimationEnd (event) {
    if (event.animationName === "animateRemoveItem") {
      event.currentTarget.remove();
      event.currentTarget.removeEventListener('animationend', handleAnimationEnd);
    }
  }

  // skip the first/current gesture item and loop through all following siblings
  for (let i = 1; i < gestureItems.length; i++) {
    const currentGestureItem = gestureItems[i];
    const previousGestureItem = gestureItems[i - 1];
    // get item positions from cache
    const currentItemPosition = itemPositionCache.get(currentGestureItem);
    const previousItemPosition = itemPositionCache.get(previousGestureItem);

    currentGestureItem.addEventListener('transitionend', handleTransitionEnd);
    // calculate and transform grid item to previous grid item position
    currentGestureItem.style.setProperty('transform', `translate(
      ${previousItemPosition.x - currentItemPosition.x}px,
      ${previousItemPosition.y - currentItemPosition.y}px)`
    );
    currentGestureItem.style.setProperty('transition', 'transform 0.3s');
  }

  gestureListItem.addEventListener('animationend', handleAnimationEnd);
  gestureListItem.classList.add('gl-item-animate-remove');
}


/**
 * Returns the gesture object which gesture pattern is the closest match to the provided pattern,
 * if its deviation is below 0.1 else return null
 * Gesture items can be excluded via the second parameter
 **/
function getMostSimilarGestureByPattern (gesturePattern, excludedGestureItems = []) {
  // generator function that returns only the gesture object and filters by gesture items
  function* gestureFilter(gestureMap, excludedGestureItems) {
    for (const [gestureItem, gesture] of gestureMap) {
      if (excludedGestureItems.includes(gestureItem)) continue;
      yield gesture;
    }
  }

  const relevantGestures = gestureFilter(Gestures, excludedGestureItems);

  return getClosestGestureByPattern(
    gesturePattern,
    relevantGestures,
    0.1,
    Config.get("Settings.Gesture.matchingAlgorithm")
  );
}


/**
 * Handles the gesture item click
 * Calls the remove gesture list item function on remove button click and removes it from the config
 * Otherwise opens the clicked gesture item in the gesture popup
 **/
function onItemClick (event) {
  // if delete button received the click
  if (event.target.classList.contains('gl-remove-button')) {
    // remove gesture object and gesture list item
    Gestures.delete(this);
    removeGestureListItem(this);
    // update config
    // this works because the config manager calls JSON.stringify which in turn calls the toJSON function of the Gesture class
    // source: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/JSON/stringify#toJSON_behavior
    Config.set("Gestures", Array.from(Gestures.values()));
  }
  else {
    // open gesture popup and hold reference to current item
    currentItem = this;
    // open gesture popup and pass related gesture object
    openGesturePopup( Gestures.get(this) );
  }
}


/**
 * Handles the gesture item hover and triggers the demo animation
 **/
function onItemPointerenter (event) {
  if (!this.classList.contains("demo") ) {
    // add delay so it only triggers if the mouse stays on the item
    setTimeout(() => {
      if (this.matches(":hover")) this.classList.add("demo");
    }, 200);
  }
}


/**
 * Handles the gesture item mouse leave and removes the demo animation
 **/
function onItemPointerleave (event) {
  const animations = this.querySelector(".gl-thumbnail-trail").getAnimations();
  const animationsAreRunning = animations.some(animation => animation.playState === "running");
  if (this.classList.contains("demo")) {
    if (!animationsAreRunning) {
      this.classList.remove("demo");
    }
    else {
      this.querySelector(".gl-thumbnail-trail").addEventListener("animationend", () => this.classList.remove("demo"), { once: true });
    }
  }
}


/**
 * Handles the input events of the search field and hides all unmatching gestures
 **/
function onSearchInput () {
  const gestureList = document.getElementById("gestureContainer");
  const gestureAddButtonItem = gestureList.firstElementChild;
  const searchQuery = document.getElementById("gestureSearchInput").value.toLowerCase().trim();
  const searchQueryKeywords = searchQuery.split(" ");

  for (const [gestureListItem, gesture] of Gestures) {
    // get the gesture string and transform all letters to lower case
    const gestureString = gesture.toString().toLowerCase();
    // check if all keywords are matching the command name
    const isMatching = searchQueryKeywords.every(keyword => gestureString.includes(keyword));
    // hide all unmatching commands and show all matching commands
    gestureListItem.hidden = !isMatching;
  }

  // hide gesture add button item on search input
  gestureAddButtonItem.hidden = !!searchQuery;

  // toggle "no search results" hint if all items are hidden
  gestureList.classList.toggle("empty", !gestureList.querySelectorAll(".gl-item:not([hidden])").length);
}


/**
 * Handles visibility of the the search field
 **/
function onSearchToggle () {
  const gestureSearchForm = document.getElementById("gestureSearchForm");
  const gestureSearchInput = document.getElementById("gestureSearchInput");

  if (gestureSearchForm.classList.toggle("show")) {
    gestureSearchInput.focus();
  }
  else {
    gestureSearchForm.reset();
    onSearchInput();
  }
}


/**
 * Handles the new gesture button click and opens the empty gesture popup
 **/
function onAddButtonClick (event) {
  currentItem = null;
  openGesturePopup();
}


/**
 * Handles the gesture popup command select change and adjusts the label input placeholder based on its current value
 **/
function onCommandSelectChange (event) {
  const gesturePopupLabelInput = document.getElementById("gesturePopupLabelInput");
  gesturePopupLabelInput.placeholder = browser.i18n.getMessage(`commandLabel${this.value.name}`);
}


/**
 * Gathers and saves the specified settings data from the input elements and closes the coommand bar
 **/
function onGesturePopupFormSubmit (event) {
  // prevent page reload
  event.preventDefault();

  const gesturePopupCommandSelect = document.getElementById("gesturePopupCommandSelect");
  const gesturePopupLabelInput = document.getElementById("gesturePopupLabelInput");

  // exit function if command select is empty or no pattern exists
  if (!gesturePopupCommandSelect.value || !currentPopupPattern) return;

  // if no item is active create a new one
  if (!currentItem) {
    // create new gesture
    const newGesture = new Gesture(currentPopupPattern, gesturePopupCommandSelect.command, gesturePopupLabelInput.value);
    // create corresponding html item
    const gestureListItem = createGestureListItem(newGesture);
    // store new gesture
    Gestures.set(gestureListItem, newGesture);
    // update config
    // this works because the config manager calls JSON.stringify which in turn calls the toJSON function of the Gesture class
    // source: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/JSON/stringify#toJSON_behavior
    Config.set("Gestures", Array.from(Gestures.values()));
    // append html item
    addGestureListItem(gestureListItem);
  }
  else {
    const currentGesture = Gestures.get(currentItem);
    // update gesture data
    currentGesture.setPattern(currentPopupPattern);
    currentGesture.setCommand(gesturePopupCommandSelect.command);
    currentGesture.setLabel(gesturePopupLabelInput.value);
    // update config
    // this works because the config manager calls JSON.stringify which in turn calls the toJSON function of the Gesture class
    // source: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/JSON/stringify#toJSON_behavior
    Config.set("Gestures", Array.from(Gestures.values()));
    // update html item
    updateGestureListItem(currentItem, currentGesture);
  }

  const gesturePopup = document.getElementById("gesturePopup");
  gesturePopup.open = false;
}


/**
 * Disables the mouse gesture controller when the popup closes and clear the pattern
 **/
function onGesturePopupClose () {
  MouseGestureController.disable();
  // clear recorded gesture pattern
  currentPopupPattern = null;

  const gesturePopupPatternContainer = document.getElementById("gesturePopupPatternContainer");
  const gesturePopupHeading = document.getElementById("gesturePopupHeading");
  const gesturePopupCommandSelect = document.getElementById("gesturePopupCommandSelect");
  const gesturePopupLabelInput = document.getElementById("gesturePopupLabelInput");

  // reset gesture popup
  gesturePopupHeading.textContent = browser.i18n.getMessage('gesturePopupTitleNewGesture');
  gesturePopupCommandSelect.command = null;
  gesturePopupLabelInput.value = "";
  gesturePopupLabelInput.placeholder = "";
  // clear popup gesture pattern if any
  if (gesturePopupPatternContainer.firstChild) gesturePopupPatternContainer.firstChild.remove();
}


/**
 * Opens the gesture popup and fills in the given values if any
 **/
function openGesturePopup (gesture = null) {
  const gesturePopupHeading = document.getElementById("gesturePopupHeading");
  const gesturePopupCommandSelect = document.getElementById("gesturePopupCommandSelect");
  const gesturePopupLabelInput = document.getElementById("gesturePopupLabelInput");
  // setup recording area
  const currentUserMouseButton = Config.get("Settings.Gesture.mouseButton");
  const mouseButtonLabelMap = {
    1: 'gesturePopupMouseButtonLeft',
    2: 'gesturePopupMouseButtonRight',
    4: 'gesturePopupMouseButtonMiddle'
  }
  const gesturePopupPatternContainer = document.getElementById("gesturePopupPatternContainer");
        gesturePopupPatternContainer.classList.remove("alert");
        gesturePopupPatternContainer.dataset.gestureRecordingHint = browser.i18n.getMessage(
          'gesturePopupRecordingAreaText',
          browser.i18n.getMessage(mouseButtonLabelMap[currentUserMouseButton])
        );
        gesturePopupPatternContainer.title = "";

  MouseGestureController.mouseButton = currentUserMouseButton;
  MouseGestureController.enable();
  // fill current values if any
  if (gesture) {
    gesturePopupHeading.textContent = browser.i18n.getMessage('gesturePopupTitleEditGesture');
    gesturePopupCommandSelect.command = gesture.getCommand();
    gesturePopupLabelInput.placeholder = gesture.getCommand().toString();
    gesturePopupLabelInput.value = gesture.getLabel();
    currentPopupPattern = gesture.getPattern();
    // add popup gesture pattern
    const gestureThumbnail = createGestureThumbnail(gesture.getPattern());
    gesturePopupPatternContainer.append(gestureThumbnail);

    // check if there is a very similar gesture and get it
    const mostSimilarGesture = getMostSimilarGestureByPattern(currentPopupPattern, [currentItem]);

    // if there is a similar gesture report it to the user
    if (mostSimilarGesture) {
      // activate alert symbol and change title
      gesturePopupPatternContainer.classList.add("alert");
      gesturePopupPatternContainer.title = browser.i18n.getMessage(
        'gesturePopupNotificationSimilarGesture',
        mostSimilarGesture.toString()
      );
    }
    else {
      gesturePopupPatternContainer.classList.remove("alert");
      gesturePopupPatternContainer.title = gesturePopupPatternContainer.dataset.gestureRecordingHint;
    }
  }

  // open popup
  const gesturePopup = document.getElementById("gesturePopup");
        gesturePopup.open = true;
}


/**
 * Adds necessary event listeners to the mouse gesture controller
 **/
function mouseGestureControllerSetup () {
  const gesturePopupCanvas = document.getElementById("gesturePopupCanvas");
  const canvasContext = gesturePopupCanvas.getContext("2d");

  MouseGestureController.addEventListener("start", (event, events) => {
    // detect if the gesture started on the recording area
    if (!event.target.closest("#gesturePopupRecordingArea")) {
      // cancel gesture and event handler if the first click was not within the recording area
      MouseGestureController.cancel();
      return;
    }

    // initialize canvas properties (correct width and height are only known after the popup has been opened)
    gesturePopupCanvas.width = gesturePopupCanvas.offsetWidth;
    gesturePopupCanvas.height = gesturePopupCanvas.offsetHeight;
    canvasContext.lineCap = "round";
    canvasContext.lineJoin = "round";
    canvasContext.lineWidth = 10;
    canvasContext.strokeStyle = "#00aaa0";

    // get first event and remove it from the array
    const firstEvent = events.shift();
    const lastEvent = events[events.length - 1] || firstEvent;
    // translate the canvas coordiantes by the position of the canvas element
    const clientRect = gesturePopupCanvas.getBoundingClientRect();
    canvasContext.setTransform(1, 0, 0, 1, -clientRect.x, -clientRect.y);
    // dradditionalArrowWidth all occurred events
    canvasContext.beginPath();
    canvasContext.moveTo(
      firstEvent.clientX,
      firstEvent.clientY
    );
    for (let event of events) canvasContext.lineTo(
      event.clientX,
      event.clientY
    );
    canvasContext.stroke();

    canvasContext.beginPath();
    canvasContext.moveTo(
      lastEvent.clientX,
      lastEvent.clientY
    );
  });

  MouseGestureController.addEventListener("update", (event) => {
    // fallback if getCoalescedEvents is not defined + https://bugzilla.mozilla.org/show_bug.cgi?id=1450692
    const events = event.getCoalescedEvents ? event.getCoalescedEvents() : [];
    if (!events.length) events.push(event);

    const lastEvent = events[events.length - 1];
    for (let event of events) canvasContext.lineTo(
      event.clientX,
      event.clientY
    );
    canvasContext.stroke();

    canvasContext.beginPath();
    canvasContext.moveTo(
      lastEvent.clientX,
      lastEvent.clientY
    );
  });

  MouseGestureController.addEventListener("abort", (event) => {
    // clear canvas
    canvasContext.setTransform(1, 0, 0, 1, 0, 0);
    canvasContext.clearRect(0, 0, gesturePopupCanvas.width, gesturePopupCanvas.height);
  });

  MouseGestureController.addEventListener("end", (event, events) => {
    // clear canvas
    canvasContext.setTransform(1, 0, 0, 1, 0, 0);
    canvasContext.clearRect(0, 0, gesturePopupCanvas.width, gesturePopupCanvas.height);

    // setup pattern extractor
    const patternConstructor = new PatternConstructor(
      Config.get("Settings.Gesture.patternDifferenceThreshold") ?? 0.12,
      Config.get("Settings.Gesture.patternDistanceThreshold") ?? 10
    );
    // gather all events in one array
    events = events.flatMap(event => {
      // fallback if getCoalescedEvents is not defined + https://bugzilla.mozilla.org/show_bug.cgi?id=1450692
      const events = event.getCoalescedEvents ? event.getCoalescedEvents() : [];
      if (!events.length) events.push(event);
      return events;
    });

    // build gesture pattern
    for (const event of events) {
      patternConstructor.addPoint(event.clientX, event.clientY);
    }
    // update current pattern
    currentPopupPattern = patternConstructor.getPattern();

    // update popup gesture pattern
    const gestureThumbnail = createGestureThumbnail(currentPopupPattern);
    const gesturePopupPatternContainer = document.getElementById("gesturePopupPatternContainer");
    // remove previous pattern if any
    if (gesturePopupPatternContainer.firstChild) gesturePopupPatternContainer.firstChild.remove();
    gesturePopupPatternContainer.append(gestureThumbnail);

    // check if there is a very similar gesture and get it
    const mostSimilarGesture = getMostSimilarGestureByPattern(currentPopupPattern, [currentItem]);

    // if there is a similar gesture report it to the user
    if (mostSimilarGesture) {
      // activate alert symbol and change title
      gesturePopupPatternContainer.classList.add("alert");
      gesturePopupPatternContainer.title = browser.i18n.getMessage(
        'gesturePopupNotificationSimilarGesture',
        mostSimilarGesture.toString()
      );
    }
    else {
      gesturePopupPatternContainer.classList.remove("alert");
      gesturePopupPatternContainer.title = gesturePopupPatternContainer.dataset.gestureRecordingHint;
    }
  });
}