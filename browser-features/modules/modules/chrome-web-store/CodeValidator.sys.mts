/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Code Validator - Checks for unsupported code patterns in Chrome extensions
 */

/**
 * Validate source code for unsupported patterns
 * Returns an error message if unsupported code is found, null otherwise
 */
export function validateSourceCode(
  filename: string,
  content: string,
): string | null {
  // Check for documentId usage in API calls
  // Firefox does not support 'documentId' in tabs.sendMessage and other APIs
  // Pattern matches "documentId:" or "documentId :" which is typical in options objects
  // Also matches "documentId" string literal which might be used in property access or object keys

  // Debug logging
  const index = content.indexOf("documentId");
  if (index !== -1) {
    const snippet = content.substring(
      Math.max(0, index - 30),
      Math.min(content.length, index + 30),
    );
    console.log(
      `[CodeValidator] Found 'documentId' in ${filename} at index ${index}. Snippet: ...${snippet}...`,
    );
  }

  if (
    /["']documentId["']\s*:/.test(content) ||
    /documentId\s*:/.test(content) ||
    /\.documentId/.test(content)
  ) {
    console.log(
      `[CodeValidator] Detected unsupported documentId usage in ${filename}`,
    );
    return "Uses unsupported 'documentId' property in API calls";
  }

  return null;
}
