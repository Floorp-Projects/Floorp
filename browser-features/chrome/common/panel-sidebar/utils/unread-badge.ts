// SPDX-License-Identifier: MPL-2.0

// Require a recognized product name as well as a numeric prefix. Titles are
// controlled by the site and may otherwise contain numbers unrelated to unread
// messages. Keep the output bounded and never render the title itself.
const services = [
  [
    "https://messages.google.com",
    /^\/web(?:\/|$)/,
    /^(?:Messages|Google Messages(?: for web: Conversations)?)$/i,
  ],
  ["https://chat.google.com", /^\//, /^(?:Google )?Chat$/i],
  ["https://mail.google.com", /^\/chat(?:\/|$)/, /^(?:Google )?Chat$/i],
  ["https://www.messenger.com", /^\//, /^Messenger$/i],
  [
    "https://www.facebook.com",
    /^\/messages(?:\/|$)/,
    /^(?:Facebook|Messenger(?: \| Facebook)?)$/i,
  ],
  ["https://web.whatsapp.com", /^\//, /^WhatsApp(?: Web)?$/i],
  ["https://www.instagram.com", /^\/direct(?:\/|$)/, /^Instagram$/i],
  ["https://voice.google.com", /^\//, /^(?:Google )?Voice$/i],
] as const;

/** Match an approved HTTPS origin and messaging route. */
function getService(spec: string) {
  const url = URL.parse(spec);
  if (!url || url.protocol !== "https:" || url.username || url.password) {
    return undefined;
  }
  return services.find(([origin, path]) =>
    url.origin === origin && path.test(url.pathname)
  );
}

/** Check whether a configured web panel can supply badge signals. */
export function supportsUnreadBadge(configuredURL: string): boolean {
  return getService(configuredURL) !== undefined;
}

// Gecko has already loaded and validated this icon for the inner tab.
// Only mirror in-memory raster data. In particular, never load a page-supplied
// remote URL or an SVG into browser chrome.
/** Return a bounded, already-loaded raster icon for eligible Google panels. */
export function getUnreadFavicon(
  configuredURL: string,
  documentURL: string,
  icon: string | null | undefined,
): string | undefined {
  const service = getService(configuredURL);
  if (
    !service || getService(documentURL) !== service ||
    (service[0] !== "https://messages.google.com" &&
      service[0] !== "https://chat.google.com" &&
      service[0] !== "https://mail.google.com") ||
    !icon || icon.length > 256 * 1024
  ) {
    return undefined;
  }
  return /^data:image\/(?:png|x-icon|vnd\.microsoft\.icon);base64,[a-z0-9+/]+=*$/i
      .test(icon)
    ? icon
    : undefined;
}

/** Read a bounded title count, explicit clear, or missing signal. */
export function getUnreadBadge(
  configuredURL: string,
  documentURL: string,
  title: string,
): string | undefined {
  const service = getService(configuredURL);
  if (!service || getService(documentURL) !== service) {
    return "";
  }

  if (title.length > 128) return undefined;

  // On the Messages view, Voice uses this exact title for an empty count.
  if (
    service[0] === "https://voice.google.com" &&
    /^(?:\/u\/\d+)?\/messages\/?$/.test(new URL(documentURL).pathname) &&
    title === "Voice - Messages"
  ) {
    return "";
  }

  // Voice places the count between the product name and the section name.
  const voiceMessages = service[0] === "https://voice.google.com"
    ? /^Voice - (\([0-9]{1,6}\+?\)) Messages$/u.exec(title)
    : null;
  const countTitle = voiceMessages ? `${voiceMessages[1]} Voice` : title;
  const match = /^\(([0-9]{1,6})(\+?)\)\s+(.+)$/u.exec(countTitle);
  if (!match || !service[2].test(match[3])) return undefined;

  const count = Number(match[1]);
  if (count === 0) return match[2] ? undefined : "";
  if (count > 99 || (count === 99 && match[2])) return "99+";
  if (match[2]) {
    const messenger = service[0] === "https://www.facebook.com" ||
      service[0] === "https://www.messenger.com";
    return messenger ? `${count}+` : undefined;
  }
  return String(count);
}

// undefined means the eligible page currently exposes no count. An empty
// string is an explicit zero or a loss of the eligible browsing context.
/** Retain the last count and suppress a count the user dismissed. */
export function createUnreadBadgeState() {
  let lastCount = "";
  let dismissedCount: string | undefined;
  return {
    update(count: string | undefined): string {
      if (count !== undefined && count !== lastCount) {
        lastCount = count;
        dismissedCount = undefined;
      }
      return lastCount === dismissedCount ? "" : lastCount;
    },
    clear(count = lastCount): void {
      lastCount = count;
      dismissedCount = count;
    },
  };
}
