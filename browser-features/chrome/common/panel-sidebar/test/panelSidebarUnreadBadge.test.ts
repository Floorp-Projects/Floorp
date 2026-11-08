// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  createUnreadBadgeState,
  getUnreadBadge as readBadgeSignal,
  getUnreadFavicon,
  supportsUnreadBadge,
} from "../utils/unread-badge.ts";
import { assertEquals, runTests } from "../../../test/utils/test_harness.ts";

const whatsapp = "https://web.whatsapp.com/";

function getUnreadBadge(
  configuredURL: string,
  documentURL: string,
  title: string,
): string {
  return readBadgeSignal(configuredURL, documentURL, title) ?? "";
}

/** Run the parser, origin validation, and dismissal regressions. */
export async function runAllTests(): Promise<void> {
  await runTests("panelSidebarUnreadBadge.test.ts", [
    {
      name:
        "mirrors raster tab icons only for the supported Google messaging routes",
      fn: () => {
        const messages = "https://messages.google.com/web/conversations";
        const chat = "https://chat.google.com/room/example";
        const png = "data:image/png;base64,AAAA";
        assertEquals(
          getUnreadFavicon(messages, messages, png),
          png,
          "Messages",
        );
        assertEquals(getUnreadFavicon(chat, chat, png), png, "Chat");
        assertEquals(
          getUnreadFavicon(messages, chat, png),
          undefined,
          "different origin",
        );
        assertEquals(
          getUnreadFavicon(whatsapp, whatsapp, png),
          undefined,
          "other service",
        );
        assertEquals(
          getUnreadFavicon(messages, messages, "https://example.org/icon.png"),
          undefined,
          "remote URL",
        );
        assertEquals(
          getUnreadFavicon(
            messages,
            messages,
            "data:image/svg+xml;base64,AAAA",
          ),
          undefined,
          "SVG",
        );
      },
    },
    {
      name:
        "retains a count through missing signals and honors manual and explicit clears",
      fn: () => {
        const state = createUnreadBadgeState();
        const update = (title: string) =>
          state.update(readBadgeSignal(whatsapp, whatsapp, title));
        assertEquals(update("(2) WhatsApp"), "2", "records a count");
        assertEquals(update("WhatsApp"), "2", "missing count preserves badge");
        state.clear();
        assertEquals(update("WhatsApp"), "", "dismisses remembered count");
        assertEquals(
          update("(2) WhatsApp"),
          "",
          "same count remains dismissed",
        );
        assertEquals(update("(3) WhatsApp"), "3", "changed count reappears");
        assertEquals(update("(0) WhatsApp"), "", "explicit zero clears");
        assertEquals(update("(3) WhatsApp"), "3", "count reappears after zero");
        assertEquals(
          state.update(readBadgeSignal(whatsapp, "about:blank", "")),
          "",
          "navigation clears",
        );
      },
    },
    {
      name: "recognizes bounded counts for each supported service",
      fn: () => {
        const examples = [
          ["https://messages.google.com/web/conversations", "Google Messages"],
          ["https://chat.google.com/room/example", "Google Chat"],
          ["https://mail.google.com/chat/u/0/", "Chat"],
          ["https://www.messenger.com/t/example", "Messenger"],
          ["https://www.facebook.com/messages/t/example", "Facebook"],
          ["https://web.whatsapp.com/", "WhatsApp"],
          ["https://www.instagram.com/direct/inbox/", "Instagram"],
          ["https://voice.google.com/u/0/messages", "Google Voice"],
        ];
        for (const [url, product] of examples) {
          assertEquals(supportsUnreadBadge(url), true, product);
          assertEquals(
            getUnreadBadge(url, url, `(7) ${product}`),
            "7",
            product,
          );
        }
      },
    },
    {
      name: "dismissing a restored count suppresses an identical live count",
      fn: () => {
        const state = createUnreadBadgeState();
        state.clear("3");
        assertEquals(state.update(undefined), "", "no live signal yet");
        assertEquals(state.update("3"), "", "same count stays dismissed");
        assertEquals(state.update("4"), "4", "new count displays");
      },
    },
    {
      name: "recognizes Messenger counts capped by Facebook",
      fn: () => {
        const facebook = "https://www.facebook.com/messages/t/";
        const messenger = "https://www.messenger.com/";
        assertEquals(
          getUnreadBadge(facebook, facebook, "(20+) Messenger | Facebook"),
          "20+",
          "live Facebook title",
        );
        assertEquals(
          getUnreadBadge(facebook, facebook, "(2) Messenger | Facebook"),
          "2",
          "exact Facebook count",
        );
        assertEquals(
          getUnreadBadge(messenger, messenger, "(20+) Messenger"),
          "20+",
          "standalone Messenger",
        );
        assertEquals(
          getUnreadBadge(
            facebook,
            "https://www.facebook.com/",
            "(20+) Messenger | Facebook",
          ),
          "",
          "requires messages route",
        );
      },
    },
    {
      name: "recognizes the live Google Messages Conversations title",
      fn: () => {
        const url = "https://messages.google.com/web/u/0/conversations";
        assertEquals(
          getUnreadBadge(
            url,
            url,
            "(1) Google Messages for web: Conversations",
          ),
          "1",
          "unread",
        );
        assertEquals(
          getUnreadBadge(url, url, "Google Messages for web: Conversations"),
          "",
          "no count",
        );
      },
    },
    {
      name: "recognizes the live Google Voice Messages title",
      fn: () => {
        const voice = "https://voice.google.com/u/1/messages";
        const state = createUnreadBadgeState();
        state.update(readBadgeSignal(voice, voice, "Voice - (1) Messages"));
        assertEquals(
          state.update(
            readBadgeSignal(
              voice,
              "https://voice.google.com/u/1/calls",
              "Voice - Calls",
            ),
          ),
          "1",
          "switching to Calls preserves the message count",
        );
        assertEquals(
          state.update(readBadgeSignal(voice, voice, "Voice - Messages")),
          "",
          "the empty Messages title clears a remembered count",
        );
        assertEquals(
          getUnreadBadge(voice, voice, "Voice - (1) Messages"),
          "1",
          "message count",
        );
        assertEquals(
          getUnreadBadge(voice, voice, "Voice - (9) Voicemail"),
          "",
          "excludes voicemail",
        );
        assertEquals(
          getUnreadBadge(voice, voice, "Voice - Messages"),
          "",
          "clears absent counts",
        );
        assertEquals(
          getUnreadBadge(whatsapp, whatsapp, "Voice - (1) Messages"),
          "",
          "Voice only",
        );
      },
    },
    {
      name: "distinguishes zero from missing, ambiguous and oversized titles",
      fn: () => {
        for (
          const title of [
            "",
            "WhatsApp",
            "(1) Shopping list",
            "(2) WhatsApp <img>",
            "(-3) WhatsApp",
            "(1.5) WhatsApp",
            "(1+) WhatsApp",
            "(1)" + " ".repeat(130) + "WhatsApp",
          ]
        ) {
          assertEquals(
            readBadgeSignal(whatsapp, whatsapp, title),
            undefined,
            title,
          );
        }
        assertEquals(
          readBadgeSignal(whatsapp, whatsapp, "(0) WhatsApp"),
          "",
          "explicit zero",
        );
        assertEquals(
          getUnreadBadge(whatsapp, whatsapp, "(99) WhatsApp"),
          "99",
          "largest exact count",
        );
        assertEquals(
          getUnreadBadge(whatsapp, whatsapp, "(99+) WhatsApp"),
          "99+",
          "caps high or overflow counts",
        );
        assertEquals(
          getUnreadBadge(whatsapp, whatsapp, "(1000) WhatsApp"),
          "99+",
          "caps high or overflow counts",
        );
      },
    },
    {
      name: "requires an approved configured site and matching committed route",
      fn: () => {
        for (
          const url of [
            "about:blank",
            "http://web.whatsapp.com/",
            "https://web.whatsapp.com:444/",
            "https://web.whatsapp.com.example.org/",
            "https://example.org/?web.whatsapp.com",
            "https://web.whatsapp.com@example.org/",
            "https://example@web.whatsapp.com/",
            "https://accounts.google.com/",
            "https://chat.google.com/",
            "",
          ]
        ) {
          assertEquals(getUnreadBadge(whatsapp, url, "(4) WhatsApp"), "", url);
          assertEquals(getUnreadBadge(url, whatsapp, "(4) WhatsApp"), "", url);
        }
        assertEquals(
          getUnreadBadge(
            "https://chat.google.com/",
            "https://mail.google.com/chat/",
            "(4) Chat",
          ),
          "",
          "does not use a different origin even for the same product",
        );
        const instagram = "https://www.instagram.com/direct/inbox/";
        assertEquals(
          getUnreadBadge(
            instagram,
            "https://www.instagram.com/directories/",
            "(4) Instagram",
          ),
          "",
          "rejects a similarly named path",
        );
        assertEquals(
          getUnreadBadge(
            instagram,
            "https://www.instagram.com/direct/t/123/",
            "(4) Instagram",
          ),
          "4",
          "allows navigation inside Direct",
        );
      },
    },
  ]);
}
