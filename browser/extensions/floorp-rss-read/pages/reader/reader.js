"use strict";

/* exported getPreviewHTML */
/* The HTML computed by this function is appended into a sandboxed iframe */

// Converts a number of bytes to the appropriate unit that results in a
// number that needs fewer than 4 digits
function convertByteUnits(bytes) {
  const units = ["Bytes", "KB", "MB", "GB"];
  let unitIndex = 0;

  // convert to next unit if it needs 4 digits (after rounding), but only if
  // we know the name of the next unit
  while ((bytes >= 999.5) && (unitIndex < units.length - 1)) {
    bytes /= 1024;
    unitIndex++;
  }

  // Get rid of insignificant bits by truncating to 1 or 0 decimal points
  // 0 -> 0; 1.2 -> 1.2; 12.3 -> 12.3; 123.4 -> 123; 234.5 -> 235
  bytes = bytes.toFixed((bytes > 0) && (bytes < 100) ? 1 : 0);

  return `${bytes} ${units[unitIndex]}`;
}

function displayFileName(mediaURL) {
  try {
    const url = new URL(mediaURL);
    const name = url.pathname.substring(url.pathname.lastIndexOf("/") + 1);
    if (name.length > 3) {
      return name;
    }
  } catch (e) {}

  // Fallback
  return decodeURIComponent(mediaURL);
}

function getPreviewHTML({ items }) {
  const formatter = new Intl.DateTimeFormat(undefined, {
    day: "numeric", month: "long", year: "numeric",
    hour: "numeric", minute: "numeric"});

  const container = document.createElement("main");
  for (const item of items) {
    const itemContainer = document.createElement("div");
    itemContainer.classList = "item";

    const anchor = item.url ? document.createElement("a") :
      document.createElement("strong");
    if (item.url) {
      anchor.href = item.url;
    }
    anchor.textContent = item.title;
    anchor.className = "item_title";

    const time = document.createElement("time");
    time.className = "item_date";
    const date = Date.parse(item.updated);
    if (date) {
      time.textContent = formatter.format(new Date(date));
    } else {
      time.textContent = item.updated;
    }

    const span = document.createElement("span");
    span.className = "item_desc";
    // The result of this insertion is rendered in a sandboxed iframe,
    // created in setPreviewContent! So this is safe as long as
    // the browser does not have any bugs...
    // eslint-disable-next-line no-unsanitized/property
    span.innerHTML = item.description;

    itemContainer.appendChild(anchor);
    itemContainer.appendChild(time);
    itemContainer.appendChild(span);

    if (item.media) {
      const title = document.createElement("strong");
      title.textContent = "Media";

      const list = document.createElement("ul");
      for (const media of item.media) {
        const li = document.createElement("li");

        const file = document.createElement("a");
        file.textContent = displayFileName(media.url);
        file.href = media.url;
        li.append(file);

        if (media.size > 0) {
          const size = document.createElement("span");
          size.textContent = ` (${convertByteUnits(media.size)})`;
          li.append(size);
        }

        list.append(li);
      }

      const mediaContainer = document.createElement("div");
      mediaContainer.className = "item_media";
      mediaContainer.append(title);
      mediaContainer.append(list);
      itemContainer.append(mediaContainer);
    }

    container.appendChild(itemContainer);
  }
  return container.innerHTML;
}
