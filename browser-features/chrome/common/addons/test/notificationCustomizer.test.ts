// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { NotificationCustomizer } from "../notification-customizer.ts";
import type { ChromeWebStoreInstallInfo } from "../types.ts";
import {
  type TestCase,
  assert,
  assertEquals,
} from "../../../test/utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

const SAMPLE_CWS_INFO: ChromeWebStoreInstallInfo = {
  name: "Test Chrome Extension",
  id: "test-extension-id-123",
};

const ALT_CWS_INFO: ChromeWebStoreInstallInfo = {
  name: "Alt Extension",
  id: "alt-ext-id-789",
};

/**
 * Create a mock install-confirmation notification DOM that mirrors
 * the Firefox addon-install-confirmation-notification XUL structure.
 *
 * Uses document.createXULElement for elements that the production code
 * queries by tag name (e.g. querySelector("popupnotificationcontent")).
 */
function createMockInstallConfirmationDOM(): XULElement {
  const notification = document!.createXULElement("vbox") as XULElement & {
    id: string;
  };
  notification.id = "addon-install-confirmation-notification";

  // popupnotificationcontent is queried by tag name, not class
  const popupContent = document!.createXULElement("popupnotificationcontent");
  notification.appendChild(popupContent);

  const descContainer = document!.createXULElement("vbox") as XULElement & {
    className: string;
  };
  descContainer.className = "popup-notification-description";
  notification.appendChild(descContainer);

  const content = document!.createXULElement("vbox") as XULElement & {
    id: string;
  };
  content.id = "addon-install-confirmation-content";
  const nameEl = document!.createXULElement("label") as XULElement & {
    className: string;
    setAttribute(name: string, value: string): void;
  };
  nameEl.className = "addon-install-confirmation-name";
  nameEl.setAttribute("value", "Test Addon");
  content.appendChild(nameEl);
  notification.appendChild(content);

  document!.body!.appendChild(notification as unknown as Node);
  return notification;
}

/**
 * Create a mock webext permissions notification DOM that mirrors
 * the Firefox addon-webext-permissions-notification XUL structure.
 */
function createMockWebExtPermissionDOM(): XULElement {
  const notification = document!.createXULElement("vbox") as XULElement & {
    id: string;
  };
  notification.id = "addon-webext-permissions-notification";

  // popupnotificationcontent is queried by tag name
  const popupContent = document!.createXULElement("popupnotificationcontent");
  notification.appendChild(popupContent);

  const descContainer = document!.createXULElement("vbox") as XULElement & {
    className: string;
  };
  descContainer.className = "popup-notification-description";
  const originalText = document!.createTextNode("Original permission text");
  descContainer.appendChild(originalText);
  notification.appendChild(descContainer);

  const intro = document!.createXULElement("vbox") as XULElement & {
    id: string;
  };
  intro.id = "addon-webext-perm-intro";
  intro.textContent = "Intro text";
  notification.appendChild(intro);

  document!.body!.appendChild(notification as unknown as Node);
  return notification;
}

/** Remove all test DOM elements */
function cleanupDOM(): void {
  [
    "addon-install-confirmation-notification",
    "addon-webext-permissions-notification",
  ].forEach((id) => {
    document!.getElementById(id)?.remove();
  });
}

// ---------------------------------------------------------------------------
// Tests: NotificationCustomizer construction
// ---------------------------------------------------------------------------

function testConstructorDoesNotThrow(): void {
  const customizer = new NotificationCustomizer();
  assert(customizer instanceof NotificationCustomizer, "should be an instance");
}

function testConstructorCreatesFreshInstance(): void {
  const a = new NotificationCustomizer();
  const b = new NotificationCustomizer();
  assert(a !== b, "each call should produce a new instance");
}

// ---------------------------------------------------------------------------
// Tests: customizeNotificationContent
// ---------------------------------------------------------------------------

function testCustomizeNotificationContentNoDOM(): void {
  cleanupDOM();
  const customizer = new NotificationCustomizer();
  // Should not throw when notification is absent
  customizer.customizeNotificationContent(SAMPLE_CWS_INFO);
}

function testCustomizeNotificationContentCreatesMessageElement(): void {
  cleanupDOM();
  createMockInstallConfirmationDOM();
  const customizer = new NotificationCustomizer();

  customizer.customizeNotificationContent(SAMPLE_CWS_INFO);

  const notification = document!.getElementById(
    "addon-install-confirmation-notification",
  );
  assert(notification !== null, "notification should exist");

  const message = notification!.querySelector(".chrome-web-store-message");
  assert(
    message !== null,
    "chrome-web-store-message element should be created",
  );
  assert(
    (message as HTMLElement).textContent !== "",
    "message should have text content",
  );

  cleanupDOM();
}

function testCustomizeNotificationContentDoesNotSetDataAttribute(): void {
  cleanupDOM();
  createMockInstallConfirmationDOM();
  const customizer = new NotificationCustomizer();

  customizer.customizeNotificationContent(SAMPLE_CWS_INFO);

  // customizeNotificationContent does NOT set data-cws-customized —
  // only customizeWebExtPermissionPrompt does.
  const notification = document!.getElementById(
    "addon-install-confirmation-notification",
  );
  assertEquals(
    notification?.hasAttribute("data-cws-customized"),
    false,
    "should NOT set data-cws-customized attribute",
  );

  cleanupDOM();
}

function testCustomizeNotificationContentBadgeAdded(): void {
  cleanupDOM();
  createMockInstallConfirmationDOM();
  const customizer = new NotificationCustomizer();

  customizer.customizeNotificationContent(SAMPLE_CWS_INFO);

  // Badge is added by addChromeExtensionBadge() inside
  // addon-install-confirmation-content, next to .addon-install-confirmation-name
  const content = document!.getElementById(
    "addon-install-confirmation-content",
  );
  const badge = content?.querySelector(".chrome-extension-badge");
  assert(badge !== null, "chrome-extension-badge should be added");

  cleanupDOM();
}

function testCustomizeNotificationContentCompatibilityWarningAdded(): void {
  cleanupDOM();
  createMockInstallConfirmationDOM();
  const customizer = new NotificationCustomizer();

  customizer.customizeNotificationContent(SAMPLE_CWS_INFO);

  // Warning is added inside popupnotificationcontent by addCompatibilityWarning()
  const notification = document!.getElementById(
    "addon-install-confirmation-notification",
  );
  const warning = notification?.querySelector(".chrome-extension-warning");
  assert(warning !== null, "chrome-extension-warning should be added");

  cleanupDOM();
}

function testCustomizeNotificationContentIdempotent(): void {
  cleanupDOM();
  createMockInstallConfirmationDOM();
  const customizer = new NotificationCustomizer();

  // Call twice — should not duplicate elements
  customizer.customizeNotificationContent(SAMPLE_CWS_INFO);
  customizer.customizeNotificationContent(SAMPLE_CWS_INFO);

  const badges = document!.querySelectorAll(".chrome-extension-badge");
  assertEquals(badges.length, 1, "badge should not be duplicated");

  const warnings = document!.querySelectorAll(".chrome-extension-warning");
  assertEquals(warnings.length, 1, "warning should not be duplicated");

  cleanupDOM();
}

// ---------------------------------------------------------------------------
// Tests: customizeWebExtPermissionPrompt
// ---------------------------------------------------------------------------

function testCustomizeWebExtPermissionPromptNoDOM(): void {
  cleanupDOM();
  const customizer = new NotificationCustomizer();
  // Should not throw when notification is absent
  customizer.customizeWebExtPermissionPrompt(SAMPLE_CWS_INFO);
}

function testCustomizeWebExtPermissionPromptCreatesMessage(): void {
  cleanupDOM();
  createMockWebExtPermissionDOM();
  const customizer = new NotificationCustomizer();

  customizer.customizeWebExtPermissionPrompt(SAMPLE_CWS_INFO);

  const notification = document!.getElementById(
    "addon-webext-permissions-notification",
  );
  assert(notification !== null, "notification should exist");

  const message = notification!.querySelector(".chrome-web-store-message");
  assert(
    message !== null,
    "chrome-web-store-message should be created in permission prompt",
  );
  assert(
    (message as HTMLElement).textContent !== "",
    "message should have content",
  );

  cleanupDOM();
}

function testCustomizeWebExtPermissionPromptHidesIntro(): void {
  cleanupDOM();
  createMockWebExtPermissionDOM();
  const customizer = new NotificationCustomizer();

  customizer.customizeWebExtPermissionPrompt(SAMPLE_CWS_INFO);

  const intro = document!.getElementById("addon-webext-perm-intro") as
    | (XULElement & { style: { display: string } })
    | null;
  assert(intro !== null, "intro element should exist");
  assertEquals(intro!.style.display, "none", "intro should be hidden");

  cleanupDOM();
}

function testCustomizeWebExtPermissionPromptAddsDataAttribute(): void {
  cleanupDOM();
  createMockWebExtPermissionDOM();
  const customizer = new NotificationCustomizer();

  customizer.customizeWebExtPermissionPrompt(SAMPLE_CWS_INFO);

  const notification = document!.getElementById(
    "addon-webext-permissions-notification",
  );
  assertEquals(
    notification?.getAttribute("data-cws-customized"),
    "true",
    "should set data-cws-customized attribute",
  );

  cleanupDOM();
}

function testCustomizeWebExtPermissionPromptCompatibilityWarning(): void {
  cleanupDOM();
  createMockWebExtPermissionDOM();
  const customizer = new NotificationCustomizer();

  customizer.customizeWebExtPermissionPrompt(SAMPLE_CWS_INFO);

  const warning = document!.querySelector(
    "#addon-webext-permissions-notification .chrome-extension-warning",
  );
  assert(
    warning !== null,
    "compatibility warning should be added to permission prompt",
  );

  cleanupDOM();
}

// ---------------------------------------------------------------------------
// Tests: restoreOriginalContent
// ---------------------------------------------------------------------------

function testRestoreOriginalContentNoDOM(): void {
  cleanupDOM();
  const customizer = new NotificationCustomizer();
  // Should not throw when no notification exists
  customizer.restoreOriginalContent();
}

function testRestoreOriginalContentWithoutCustomization(): void {
  cleanupDOM();
  createMockWebExtPermissionDOM();
  const customizer = new NotificationCustomizer();

  // Restore without prior customization — no data-cws-customized attribute
  customizer.restoreOriginalContent();

  const notification = document!.getElementById(
    "addon-webext-permissions-notification",
  );
  assert(
    !notification?.hasAttribute("data-cws-customized"),
    "should not add data-cws-customized",
  );

  cleanupDOM();
}

function testRestoreOriginalContentAfterCustomization(): void {
  cleanupDOM();
  createMockWebExtPermissionDOM();
  const customizer = new NotificationCustomizer();

  // Customize first
  customizer.customizeWebExtPermissionPrompt(SAMPLE_CWS_INFO);
  const notification = document!.getElementById(
    "addon-webext-permissions-notification",
  );
  assert(
    notification?.hasAttribute("data-cws-customized"),
    "should be customized",
  );

  // Restore
  customizer.restoreOriginalContent();

  assert(
    !notification?.hasAttribute("data-cws-customized"),
    "data-cws-customized should be removed after restore",
  );

  const message = notification?.querySelector(".chrome-web-store-message");
  assertEquals(message, null, "chrome-web-store-message should be removed");

  const warning = notification?.querySelector(".chrome-extension-warning");
  assertEquals(warning, null, "chrome-extension-warning should be removed");

  cleanupDOM();
}

function testRestoreOriginalContentRestoresIntro(): void {
  cleanupDOM();
  createMockWebExtPermissionDOM();
  const customizer = new NotificationCustomizer();

  // Customize hides intro
  customizer.customizeWebExtPermissionPrompt(SAMPLE_CWS_INFO);
  const intro = document!.getElementById(
    "addon-webext-perm-intro",
  ) as XULElement & { style: { display: string } };
  assertEquals(intro.style.display, "none", "intro should be hidden");

  // Restore should un-hide it
  customizer.restoreOriginalContent();
  assert(intro.style.display !== "none", "intro display should be restored");

  cleanupDOM();
}

// ---------------------------------------------------------------------------
// Tests: Round-trip (customize → restore → customize)
// ---------------------------------------------------------------------------

function testRoundTripCustomizeRestoreCustomize(): void {
  cleanupDOM();
  createMockWebExtPermissionDOM();
  const customizer = new NotificationCustomizer();
  const notification = document!.getElementById(
    "addon-webext-permissions-notification",
  );

  // First customization
  customizer.customizeWebExtPermissionPrompt(SAMPLE_CWS_INFO);
  assert(
    notification?.querySelector(".chrome-web-store-message") !== null,
    "first customize adds message",
  );

  // Restore
  customizer.restoreOriginalContent();
  assertEquals(
    notification?.querySelector(".chrome-web-store-message"),
    null,
    "restore removes message",
  );

  // Second customization with different info
  customizer.customizeWebExtPermissionPrompt(ALT_CWS_INFO);
  const message = notification?.querySelector(".chrome-web-store-message");
  assert(message !== null, "second customize adds message again");

  cleanupDOM();
}

function testMultipleCustomizeCallsPreserveOriginalChildren(): void {
  cleanupDOM();
  createMockWebExtPermissionDOM();
  const customizer = new NotificationCustomizer();
  const descContainer = document!.querySelector(
    "#addon-webext-permissions-notification .popup-notification-description",
  );
  const originalChildCount = descContainer?.childNodes.length ?? 0;

  // Customize multiple times
  customizer.customizeWebExtPermissionPrompt(SAMPLE_CWS_INFO);
  customizer.customizeWebExtPermissionPrompt(ALT_CWS_INFO);

  // Restore
  customizer.restoreOriginalContent();

  const restoredChildCount = descContainer?.childNodes.length ?? 0;
  assertEquals(
    restoredChildCount,
    originalChildCount,
    "original children count should be restored",
  );

  cleanupDOM();
}

// ---------------------------------------------------------------------------
// Tests: customizeNotificationContent - Edge Cases
// ---------------------------------------------------------------------------

function testCustomizeNotificationContentWithNullName(): void {
  cleanupDOM();
  createMockInstallConfirmationDOM();
  const customizer = new NotificationCustomizer();

  // CWS info with null name
  const cwsInfoWithNullName: ChromeWebStoreInstallInfo = {
    name: null as unknown as string,
    id: "test-id",
  };

  customizer.customizeNotificationContent(cwsInfoWithNullName);

  const message = document!.querySelector(".chrome-web-store-message");
  assert(
    message !== null,
    "should create message even with null name",
  );
  assert(
    (message as HTMLElement).textContent !== "",
    "should use default name when null",
  );

  cleanupDOM();
}

function testCustomizeNotificationContentWithUndefinedName(): void {
  cleanupDOM();
  createMockInstallConfirmationDOM();
  const customizer = new NotificationCustomizer();

  // CWS info with undefined name
  const cwsInfoWithUndefinedName: ChromeWebStoreInstallInfo = {
    name: undefined as unknown as string,
    id: "test-id",
  };

  customizer.customizeNotificationContent(cwsInfoWithUndefinedName);

  const message = document!.querySelector(".chrome-web-store-message");
  assert(
    message !== null,
    "should create message even with undefined name",
  );

  cleanupDOM();
}

function testCustomizeNotificationContentMissingPopupContent(): void {
  cleanupDOM();
  const customizer = new NotificationCustomizer();

  // Create notification without popupnotificationcontent
  const notification = document!.createXULElement("vbox") as XULElement & {
    id: string;
  };
  notification.id = "addon-install-confirmation-notification";
  document!.body!.appendChild(notification as unknown as Node);

  // Should not throw
  customizer.customizeNotificationContent(SAMPLE_CWS_INFO);

  cleanupDOM();
}

function testCustomizeNotificationContentMissingDescriptionContainer(): void {
  cleanupDOM();
  const customizer = new NotificationCustomizer();

  // Create notification without description container
  const notification = document!.createXULElement("vbox") as XULElement & {
    id: string;
  };
  notification.id = "addon-install-confirmation-notification";

  const popupContent = document!.createXULElement("popupnotificationcontent");
  notification.appendChild(popupContent);

  document!.body!.appendChild(notification as unknown as Node);

  // Should not throw
  customizer.customizeNotificationContent(SAMPLE_CWS_INFO);

  cleanupDOM();
}

function testCustomizeNotificationContentReusesExistingMessage(): void {
  cleanupDOM();
  createMockInstallConfirmationDOM();
  const customizer = new NotificationCustomizer();

  // First customization
  customizer.customizeNotificationContent(SAMPLE_CWS_INFO);
  const _firstMessage = document!.querySelector(".chrome-web-store-message");

  // Second customization
  customizer.customizeNotificationContent(ALT_CWS_INFO);
  const _secondMessage = document!.querySelector(".chrome-web-store-message");

  // Should reuse the same element, not create a duplicate
  const allMessages = document!.querySelectorAll(".chrome-web-store-message");
  assertEquals(
    allMessages.length,
    1,
    "should reuse existing message element",
  );

  cleanupDOM();
}

// ---------------------------------------------------------------------------
// Tests: customizeWebExtPermissionPrompt - Edge Cases
// ---------------------------------------------------------------------------

function testCustomizeWebExtPermissionPromptWithNullName(): void {
  cleanupDOM();
  createMockWebExtPermissionDOM();
  const customizer = new NotificationCustomizer();

  const cwsInfoWithNullName: ChromeWebStoreInstallInfo = {
    name: null as unknown as string,
    id: "test-id",
  };

  customizer.customizeWebExtPermissionPrompt(cwsInfoWithNullName);

  const message = document!.querySelector(".chrome-web-store-message");
  assert(
    message !== null,
    "should create message even with null name",
  );
  assert(
    (message as HTMLElement).textContent !== "",
    "should use default name when null",
  );

  cleanupDOM();
}

function testCustomizeWebExtPermissionPromptWithUndefinedName(): void {
  cleanupDOM();
  createMockWebExtPermissionDOM();
  const customizer = new NotificationCustomizer();

  const cwsInfoWithUndefinedName: ChromeWebStoreInstallInfo = {
    name: undefined as unknown as string,
    id: "test-id",
  };

  customizer.customizeWebExtPermissionPrompt(cwsInfoWithUndefinedName);

  const message = document!.querySelector(".chrome-web-store-message");
  assert(
    message !== null,
    "should create message even with undefined name",
  );

  cleanupDOM();
}

function testCustomizeWebExtPermissionPromptMissingPopupContent(): void {
  cleanupDOM();
  const customizer = new NotificationCustomizer();

  // Create notification without popupnotificationcontent
  const notification = document!.createXULElement("vbox") as XULElement & {
    id: string;
  };
  notification.id = "addon-webext-permissions-notification";

  const descContainer = document!.createXULElement("vbox") as XULElement & {
    className: string;
  };
  descContainer.className = "popup-notification-description";
  notification.appendChild(descContainer);

  document!.body!.appendChild(notification as unknown as Node);

  // Should not throw
  customizer.customizeWebExtPermissionPrompt(SAMPLE_CWS_INFO);

  cleanupDOM();
}

function testCustomizeWebExtPermissionPromptMissingDescriptionContainer(): void {
  cleanupDOM();
  const customizer = new NotificationCustomizer();

  // Create notification without description container
  const notification = document!.createXULElement("vbox") as XULElement & {
    id: string;
  };
  notification.id = "addon-webext-permissions-notification";

  const popupContent = document!.createXULElement("popupnotificationcontent");
  notification.appendChild(popupContent);

  document!.body!.appendChild(notification as unknown as Node);

  // Should not throw
  customizer.customizeWebExtPermissionPrompt(SAMPLE_CWS_INFO);

  cleanupDOM();
}

function testCustomizeWebExtPermissionPromptMissingIntro(): void {
  cleanupDOM();
  const customizer = new NotificationCustomizer();

  // Create notification without intro element
  const notification = document!.createXULElement("vbox") as XULElement & {
    id: string;
  };
  notification.id = "addon-webext-permissions-notification";

  const popupContent = document!.createXULElement("popupnotificationcontent");
  notification.appendChild(popupContent);

  const descContainer = document!.createXULElement("vbox") as XULElement & {
    className: string;
  };
  descContainer.className = "popup-notification-description";
  notification.appendChild(descContainer);

  document!.body!.appendChild(notification as unknown as Node);

  // Should not throw
  customizer.customizeWebExtPermissionPrompt(SAMPLE_CWS_INFO);

  cleanupDOM();
}

function testCustomizeWebExtPermissionPromptReusesExistingMessage(): void {
  cleanupDOM();
  createMockWebExtPermissionDOM();
  const customizer = new NotificationCustomizer();

  // First customization
  customizer.customizeWebExtPermissionPrompt(SAMPLE_CWS_INFO);
  const _firstMessage = document!.querySelector(".chrome-web-store-message");

  // Second customization
  customizer.customizeWebExtPermissionPrompt(ALT_CWS_INFO);
  const _secondMessage = document!.querySelector(".chrome-web-store-message");

  // Should reuse the same element, not create a duplicate
  const allMessages = document!.querySelectorAll(".chrome-web-store-message");
  assertEquals(
    allMessages.length,
    1,
    "should reuse existing message element",
  );

  cleanupDOM();
}

// ---------------------------------------------------------------------------
// Tests: restoreOriginalContent - Edge Cases
// ---------------------------------------------------------------------------

function testRestoreOriginalContentMissingNotification(): void {
  cleanupDOM();
  const customizer = new NotificationCustomizer();

  // Should not throw when notification doesn't exist
  customizer.restoreOriginalContent();
}

function testRestoreOriginalContentMissingDescriptionContainer(): void {
  cleanupDOM();
  const customizer = new NotificationCustomizer();

  // Create notification without description container
  const notification = document!.createXULElement("vbox") as XULElement & {
    id: string;
  };
  notification.id = "addon-webext-permissions-notification";
  notification.setAttribute("data-cws-customized", "true");
  document!.body!.appendChild(notification as unknown as Node);

  // Should not throw
  customizer.restoreOriginalContent();

  cleanupDOM();
}

function testRestoreOriginalContentMissingIntro(): void {
  cleanupDOM();
  const customizer = new NotificationCustomizer();

  // Create notification without intro element
  const notification = document!.createXULElement("vbox") as XULElement & {
    id: string;
  };
  notification.id = "addon-webext-permissions-notification";
  notification.setAttribute("data-cws-customized", "true");

  const popupContent = document!.createXULElement("popupnotificationcontent");
  notification.appendChild(popupContent);

  const descContainer = document!.createXULElement("vbox") as XULElement & {
    className: string;
  };
  descContainer.className = "popup-notification-description";
  notification.appendChild(descContainer);

  document!.body!.appendChild(notification as unknown as Node);

  // Should not throw
  customizer.restoreOriginalContent();

  cleanupDOM();
}

function testRestoreOriginalContentIdempotent(): void {
  cleanupDOM();
  createMockWebExtPermissionDOM();
  const customizer = new NotificationCustomizer();

  // Customize
  customizer.customizeWebExtPermissionPrompt(SAMPLE_CWS_INFO);

  // Restore multiple times
  customizer.restoreOriginalContent();
  customizer.restoreOriginalContent();
  customizer.restoreOriginalContent();

  // Should not throw and state should be consistent
  const notification = document!.getElementById(
    "addon-webext-permissions-notification",
  );
  assertEquals(
    notification?.hasAttribute("data-cws-customized"),
    false,
    "should not have customization attribute",
  );

  cleanupDOM();
}

// ---------------------------------------------------------------------------
// Tests: addChromeExtensionBadge - Multiple Name Elements
// ---------------------------------------------------------------------------

function testAddChromeExtensionBadgeMultipleNameElements(): void {
  cleanupDOM();
  const notification = document!.createXULElement("vbox") as XULElement & {
    id: string;
  };
  notification.id = "addon-install-confirmation-notification";

  const popupContent = document!.createXULElement("popupnotificationcontent");
  notification.appendChild(popupContent);

  const content = document!.createXULElement("vbox") as XULElement & {
    id: string;
  };
  content.id = "addon-install-confirmation-content";

  // Add multiple name elements
  const nameEl1 = document!.createXULElement("label") as XULElement & {
    className: string;
  };
  nameEl1.className = "addon-install-confirmation-name";
  const parent1 = document!.createElement("div");
  parent1.appendChild(nameEl1);
  content.appendChild(parent1);

  const nameEl2 = document!.createXULElement("label") as XULElement & {
    className: string;
  };
  nameEl2.className = "addon-install-confirmation-name";
  const parent2 = document!.createElement("div");
  parent2.appendChild(nameEl2);
  content.appendChild(parent2);

  notification.appendChild(content);
  document!.body!.appendChild(notification as unknown as Node);

  const customizer = new NotificationCustomizer();
  customizer.customizeNotificationContent(SAMPLE_CWS_INFO);

  // Should add badge to all name elements
  const badges = content.querySelectorAll(".chrome-extension-badge");
  assertEquals(
    badges.length,
    2,
    "should add badge to all name elements",
  );

  cleanupDOM();
}

// ---------------------------------------------------------------------------
// Tests: addCompatibilityWarning - Idempotent
// ---------------------------------------------------------------------------

function testAddCompatibilityWarningIdempotent(): void {
  cleanupDOM();
  createMockInstallConfirmationDOM();
  const customizer = new NotificationCustomizer();

  // First call adds warning
  customizer.customizeNotificationContent(SAMPLE_CWS_INFO);
  const _firstWarning = document!.querySelector(".chrome-extension-warning");

  // Second call should not duplicate
  customizer.customizeNotificationContent(ALT_CWS_INFO);
  const allWarnings = document!.querySelectorAll(".chrome-extension-warning");

  assertEquals(
    allWarnings.length,
    1,
    "should not duplicate warning element",
  );

  cleanupDOM();
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    // Construction
    { name: "constructor does not throw", fn: testConstructorDoesNotThrow },
    {
      name: "constructor creates fresh instance",
      fn: testConstructorCreatesFreshInstance,
    },

    // customizeNotificationContent
    {
      name: "customizeNotificationContent - no DOM",
      fn: testCustomizeNotificationContentNoDOM,
    },
    {
      name: "customizeNotificationContent - creates message element",
      fn: testCustomizeNotificationContentCreatesMessageElement,
    },
    {
      name: "customizeNotificationContent - does NOT set data attribute",
      fn: testCustomizeNotificationContentDoesNotSetDataAttribute,
    },
    {
      name: "customizeNotificationContent - badge added",
      fn: testCustomizeNotificationContentBadgeAdded,
    },
    {
      name: "customizeNotificationContent - warning added",
      fn: testCustomizeNotificationContentCompatibilityWarningAdded,
    },
    {
      name: "customizeNotificationContent - idempotent",
      fn: testCustomizeNotificationContentIdempotent,
    },

    // customizeNotificationContent - Edge Cases
    {
      name: "customizeNotificationContent - null name",
      fn: testCustomizeNotificationContentWithNullName,
    },
    {
      name: "customizeNotificationContent - undefined name",
      fn: testCustomizeNotificationContentWithUndefinedName,
    },
    {
      name: "customizeNotificationContent - missing popup content",
      fn: testCustomizeNotificationContentMissingPopupContent,
    },
    {
      name: "customizeNotificationContent - missing description container",
      fn: testCustomizeNotificationContentMissingDescriptionContainer,
    },
    {
      name: "customizeNotificationContent - reuses existing message",
      fn: testCustomizeNotificationContentReusesExistingMessage,
    },

    // customizeWebExtPermissionPrompt
    {
      name: "customizeWebExtPermissionPrompt - no DOM",
      fn: testCustomizeWebExtPermissionPromptNoDOM,
    },
    {
      name: "customizeWebExtPermissionPrompt - creates message",
      fn: testCustomizeWebExtPermissionPromptCreatesMessage,
    },
    {
      name: "customizeWebExtPermissionPrompt - hides intro",
      fn: testCustomizeWebExtPermissionPromptHidesIntro,
    },
    {
      name: "customizeWebExtPermissionPrompt - adds data attribute",
      fn: testCustomizeWebExtPermissionPromptAddsDataAttribute,
    },
    {
      name: "customizeWebExtPermissionPrompt - warning added",
      fn: testCustomizeWebExtPermissionPromptCompatibilityWarning,
    },

    // customizeWebExtPermissionPrompt - Edge Cases
    {
      name: "customizeWebExtPermissionPrompt - null name",
      fn: testCustomizeWebExtPermissionPromptWithNullName,
    },
    {
      name: "customizeWebExtPermissionPrompt - undefined name",
      fn: testCustomizeWebExtPermissionPromptWithUndefinedName,
    },
    {
      name: "customizeWebExtPermissionPrompt - missing popup content",
      fn: testCustomizeWebExtPermissionPromptMissingPopupContent,
    },
    {
      name: "customizeWebExtPermissionPrompt - missing description container",
      fn: testCustomizeWebExtPermissionPromptMissingDescriptionContainer,
    },
    {
      name: "customizeWebExtPermissionPrompt - missing intro",
      fn: testCustomizeWebExtPermissionPromptMissingIntro,
    },
    {
      name: "customizeWebExtPermissionPrompt - reuses existing message",
      fn: testCustomizeWebExtPermissionPromptReusesExistingMessage,
    },

    // restoreOriginalContent
    {
      name: "restoreOriginalContent - no DOM",
      fn: testRestoreOriginalContentNoDOM,
    },
    {
      name: "restoreOriginalContent - without customization",
      fn: testRestoreOriginalContentWithoutCustomization,
    },
    {
      name: "restoreOriginalContent - after customization",
      fn: testRestoreOriginalContentAfterCustomization,
    },
    {
      name: "restoreOriginalContent - restores intro",
      fn: testRestoreOriginalContentRestoresIntro,
    },

    // restoreOriginalContent - Edge Cases
    {
      name: "restoreOriginalContent - missing notification",
      fn: testRestoreOriginalContentMissingNotification,
    },
    {
      name: "restoreOriginalContent - missing description container",
      fn: testRestoreOriginalContentMissingDescriptionContainer,
    },
    {
      name: "restoreOriginalContent - missing intro",
      fn: testRestoreOriginalContentMissingIntro,
    },
    {
      name: "restoreOriginalContent - idempotent",
      fn: testRestoreOriginalContentIdempotent,
    },

    // Round-trip
    {
      name: "round-trip customize → restore → customize",
      fn: testRoundTripCustomizeRestoreCustomize,
    },
    {
      name: "multiple customizes preserve original children",
      fn: testMultipleCustomizeCallsPreserveOriginalChildren,
    },

    // addChromeExtensionBadge - Multiple Elements
    {
      name: "addChromeExtensionBadge - multiple name elements",
      fn: testAddChromeExtensionBadgeMultipleNameElements,
    },

    // addCompatibilityWarning - Idempotent
    {
      name: "addCompatibilityWarning - idempotent",
      fn: testAddCompatibilityWarningIdempotent,
    },
  ];

  const failures: string[] = [];

  for (const test of tests) {
    try {
      await test.fn();
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      failures.push(`${test.name}: ${message}`);
    }
  }

  // Clean up any leftover DOM
  cleanupDOM();

  if (failures.length > 0) {
    throw new Error(
      `notificationCustomizer.test.ts failures: ${failures.join(" | ")}`,
    );
  }
}
