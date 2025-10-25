/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Parse and apply user.js preferences to Firefox
 */
export function applyUserJS(userjs: string): void {
  // Remove block comments so optional user_pref blocks stay commented out.
  const sanitizedUserJs = userjs.replace(/\/\*[\s\S]*?\*\//g, "");
  const lines = sanitizedUserJs.split("\n");
  const cleanedLines: string[] = [];
  let currentLine = "";

  // Accumulate multi-line user_pref statements
  for (const line of lines) {
    const trimmed = line.trim();

    // Skip comment-only lines and empty lines
    if (
      trimmed.startsWith("//") ||
      trimmed.startsWith("/*") ||
      trimmed.startsWith("*") ||
      trimmed === ""
    ) {
      continue;
    }

    // Accumulate lines until we hit a semicolon
    currentLine += " " + line;
    if (currentLine.includes(");")) {
      cleanedLines.push(currentLine.trim());
      currentLine = "";
    }
  }

  let appliedCount = 0;
  let skippedCount = 0;

  // Process each user_pref line
  for (const line of cleanedLines) {
    if (!line.includes("user_pref")) continue;

    try {
      // Extract content between user_pref( and );
      const match = line.match(/user_pref\s*\(\s*(.+?)\s*\)\s*;/s);
      if (!match) {
        console.warn("[applyUserJS] Could not parse:", line);
        skippedCount++;
        continue;
      }

      // Split by comma, respecting commas inside strings
      const content = match[1];
      const parts = content.split(/,(?=(?:(?:[^"]*"){2})*[^"]*$)/);

      if (parts.length < 2) {
        console.warn("[applyUserJS] Invalid format:", line);
        skippedCount++;
        continue;
      }

      const prefName = parts[0].trim().replace(/^["']|["']$/g, "");
      const value = parts.slice(1).join(",").trim();

      // Apply preference based on value type
      if (value === "true" || value === "false") {
        Services.prefs
          .getDefaultBranch("")
          .setBoolPref(prefName, value === "true");
        appliedCount++;
      } else if (value.startsWith('"') || value.startsWith("'")) {
        const stringValue = value.replace(/^["']|["']$/g, "");
        Services.prefs
          .getDefaultBranch("")
          .setStringPref(prefName, stringValue);
        appliedCount++;
      } else if (!Number.isNaN(Number(value))) {
        Services.prefs.getDefaultBranch("").setIntPref(prefName, Number(value));
        appliedCount++;
      } else {
        console.warn(
          `[applyUserJS] Unknown value type: ${prefName} = ${value}`,
        );
        skippedCount++;
      }
    } catch (error) {
      console.error("[applyUserJS] Error processing line:", line, error);
      skippedCount++;
    }
  }

  if (skippedCount > 0) {
    console.warn(
      `[applyUserJS] Applied ${appliedCount} preferences, skipped ${skippedCount}`,
    );
  }
}

/**
 * Reset preferences that were set by a user.js file
 */
export async function resetPreferencesWithUserJsContents(
  path: string,
): Promise<void> {
  const userjs = await (await fetch(path)).text();
  const regex =
    /\/\/.*?$|\/\*[\s\S]*?\*\/|^(?:[\t ]*(?:\r?\n|\r))+|\/\/\s*user_pref\(.*?\);\s*$/gm;
  const cleaned = userjs.replace(regex, "\n");

  for (const line of cleaned.split("\n")) {
    if (!line.includes("user_pref")) continue;

    const tmp = line.replaceAll("user_pref(", "").replaceAll(");", "");
    const [prefName] = tmp.split(",");
    const cleanPrefName = prefName.trim().replaceAll('"', "");

    // Skip toolkit.legacyUserProfileCustomizations.stylesheets
    // to avoid unloading userChrome.css
    if (
      cleanPrefName.startsWith(
        "toolkit.legacyUserProfileCustomizations.stylesheets",
      )
    ) {
      continue;
    }

    Services.prefs.clearUserPref(cleanPrefName);
  }
}
