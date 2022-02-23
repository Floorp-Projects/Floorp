/**
 * PopupCommandView
 * Listens for "PopupConnection" background connection and displays the message dataset
 * An iframe is used in order to protect the user data from webpages that may try to read or manipulate the contents of the popup
 **/

// save a reference to the current channel
let channel = null;

browser.runtime.onConnect.addListener(handleConnection);

// add event listeners
window.addEventListener("contextmenu", preventContextmenu, true);

window.addEventListener("pointerdown", preventAutoscroll, true);

window.addEventListener("DOMContentLoaded", handleDOMContentLoaded);

/**
 * Builds all popup html contents
 * Requires the background/command message containing the dataset
 **/
function initialize (dataset) {
  // create list and item template
  const list = document.createElement("ul");
        list.id = "list";
  const itemTemplate = document.createElement("li");
        itemTemplate.classList.add("item");
  const icon = document.createElement("img");
        // use zero width space to show alt tag on missing src
        icon.alt = "\u200B";
  const text = document.createElement("span");
  itemTemplate.append(icon, text);

  // map data to list items
  for (let element of dataset) {
    const item = itemTemplate.cloneNode(true);
          item.dataset.id = element.id;
          item.onclick = handleItemSelection;
          item.onauxclick = handleItemSelection;
    // add image icon if available
    if (element.icon) {
      item.firstElementChild.src = element.icon;
    }
    // add label
    item.lastElementChild.textContent = element.label;

    list.append(item);
  }

  // use resize observer to reliably get dimensions after reflow/layout
  // otherwise if using offsetHeight or getBoundingBox even with setTimeout
  // the dimensions of the list element sometimes equal 0
  const resizeObserver = new ResizeObserver(async (entries) => {
	  const lastEntry = entries.pop();

    // the width and height the list occupies
    const requiredDimensions = {
      // older versions of Firefox (<= 91) provided a single size object instead of an array of sizes
      // https://bugzilla.mozilla.org/show_bug.cgi?id=1689645
      width: lastEntry.borderBoxSize?.inlineSize ?? lastEntry.borderBoxSize[0].inlineSize,
      height: lastEntry.borderBoxSize?.blockSize ?? lastEntry.borderBoxSize[0].blockSize
    }

    resizeObserver.disconnect();

    // initiate popup display
    // also get the available width and height
    const availableDimensions =  await browser.runtime.sendMessage({
      subject: "popupInitiation",
      data: requiredDimensions
    });

    // focus popup frame
    window.focus();
    window.onblur = terminate;

    // add scroll buttons if list is scrollable
    if (availableDimensions.height < requiredDimensions.height) {
      const buttonUp = document.createElement("div");
            buttonUp.classList.add("button", "up", "hidden");
            buttonUp.onmouseover = handleScrollButtonMouseover;
      const buttonDown = document.createElement("div");
            buttonDown.classList.add("button", "down");
            buttonDown.onmouseover = handleScrollButtonMouseover;

      window.addEventListener("scroll", () => {
        const isOnTop = document.scrollingElement.scrollTop <= 0;
        buttonUp.classList.toggle("hidden", isOnTop);

        const isOnBottom = Math.round(document.scrollingElement.scrollTop) >= Math.round(requiredDimensions.height - availableDimensions.height);
        buttonDown.classList.toggle("hidden", isOnBottom);
      }, { passive: true })

      document.body.append(buttonUp, buttonDown);
    }
  });

  // start observing and append the list element
  resizeObserver.observe(list, { box: "border-box" });

  document.body.appendChild(list);
}


/**
 * Closes the messaging channel and sends the popup termination message to close the popup
 **/
function terminate () {
  // disconnect channel
  channel.disconnect();
  // close/remove popup
  browser.runtime.sendMessage({
    subject: "popupTermination"
  });
}


/**
 * Handles background connection requests
 **/
function handleConnection (port) {
  if (port.name === "PopupConnection") {
    // save reference to port object
    channel = port;
    // add listener
    channel.onMessage.addListener(initialize);
    channel.onDisconnect.addListener(terminate);
  }
}


/**
 * Prevents the context menu because of design reason and for rocker gesture conflicts
 **/
function preventContextmenu (event) {
  event.preventDefault();
}


/**
 * Prevents the middle click scroll on windows
 **/
function preventAutoscroll (event) {
  if (event.button === 1) event.preventDefault();
}


/**
 * Retrieves the theme from the query parameter and sets it as a global class
 **/
function handleDOMContentLoaded (event) {
  const urlParams = new URLSearchParams(window.location.search);
  const theme = urlParams.get('theme');
  if (theme) document.documentElement.classList.add(`${theme}-theme`);
}


/**
 * Passes the id of the selected item to the corresponding command
 **/
function handleItemSelection (event) {
  channel.postMessage({
    button: event.button,
    id: this.dataset.id
  });
  event.preventDefault();
}


/**
 * Handles the up and down controls
 **/
function handleScrollButtonMouseover (event) {
  const direction = this.classList.contains("up") ? -4 : 4;
  const button = event.currentTarget;
  const startTimestamp = event.timeStamp;

  function step (timestamp) {
    if (!button.matches(':hover')) return;
    // delay scroll by 0.3 seconds
    if (timestamp - startTimestamp > 300) window.scrollBy(0, direction);
    window.requestAnimationFrame(step);
  }
  window.requestAnimationFrame(step);
}