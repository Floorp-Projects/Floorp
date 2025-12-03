/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Code Validator - Checks for unsupported code patterns in Chrome extensions
 */

import type {
  ValidationResult,
  ValidationError,
  ValidationWarning,
} from "./types.ts";
import {
  UNSUPPORTED_CODE_PATTERNS,
  type UnsupportedCodePattern,
} from "./Constants.sys.mts";

// =============================================================================
// Constants
// =============================================================================

const LOG_PREFIX = "[CodeValidator]";

/** JavaScript file extensions to validate */
const JS_EXTENSIONS = [".js", ".mjs", ".jsx", ".ts", ".mts", ".tsx"] as const;

/** Quick check patterns for early detection */
const QUICK_CHECK_PATTERNS = [
  "documentId",
  "chrome.offscreen",
  "chrome.tabCapture",
  "chrome.tabGroups",
  "chrome.sidePanel",
] as const;

// =============================================================================
// Warning Patterns
// =============================================================================

/**
 * Warning-level patterns - may work but with limitations
 */
const WARNING_PATTERNS: readonly UnsupportedCodePattern[] = [
  {
    pattern: /chrome\.tabGroups\./,
    message: "chrome.tabGroups API has limited support",
  },
  {
    pattern: /chrome\.sidePanel\./,
    message: "chrome.sidePanel API is not fully supported",
  },
  {
    pattern: /chrome\.declarativeNetRequest\.MAX_NUMBER_OF_DYNAMIC_RULES/,
    message: "Dynamic rules limit differs from Chrome",
  },
];

// =============================================================================
// Public API
// =============================================================================

/**
 * Validate source code for unsupported patterns
 * @param filename - Name of the file being validated
 * @param content - Source code content
 * @returns Error message if unsupported code is found, null otherwise
 */
export function validateSourceCode(
  filename: string,
  content: string,
): string | null {
  const result = validateSourceCodeFull(filename, content);

  if (!result.valid && result.errors.length > 0) {
    return result.errors[0].message;
  }

  return null;
}

/**
 * Validate source code with detailed results
 * @param filename - Name of the file being validated
 * @param content - Source code content
 * @returns Detailed validation result
 */
export function validateSourceCodeFull(
  filename: string,
  content: string,
): ValidationResult {
  const errors: ValidationError[] = [];
  const warnings: ValidationWarning[] = [];

  // Check error-level patterns from constants
  for (const { pattern, message } of UNSUPPORTED_CODE_PATTERNS) {
    const match = pattern.exec(content);
    if (match) {
      const lineInfo = getLineInfo(content, match.index);
      errors.push({
        file: filename,
        line: lineInfo.line,
        column: lineInfo.column,
        message: `Uses unsupported '${message}'`,
        code: "UNSUPPORTED_API",
      });

      console.log(
        `${LOG_PREFIX} Detected unsupported pattern in ${filename}:${lineInfo.line}: ${message}`,
      );
    }
  }

  // Check warning-level patterns
  for (const { pattern, message } of WARNING_PATTERNS) {
    const match = pattern.exec(content);
    if (match) {
      warnings.push({
        file: filename,
        message,
      });

      console.debug(`${LOG_PREFIX} Warning in ${filename}: ${message}`);
    }
  }

  return {
    valid: errors.length === 0,
    errors,
    warnings,
  };
}

/**
 * Validate multiple files
 * @param files - Map of filename to content
 * @returns Combined validation result
 */
export function validateMultipleFiles(
  files: Map<string, string>,
): ValidationResult {
  const allErrors: ValidationError[] = [];
  const allWarnings: ValidationWarning[] = [];

  for (const [filename, content] of files) {
    // Only validate JavaScript files
    if (!isJavaScriptFile(filename)) {
      continue;
    }

    const result = validateSourceCodeFull(filename, content);
    allErrors.push(...result.errors);
    allWarnings.push(...result.warnings);
  }

  return {
    valid: allErrors.length === 0,
    errors: allErrors,
    warnings: allWarnings,
  };
}

/**
 * Check if a file should be validated based on extension
 * @param filename - Name of the file
 * @returns true if the file is a JavaScript/TypeScript file
 */
export function isJavaScriptFile(filename: string): boolean {
  return JS_EXTENSIONS.some((ext) => filename.endsWith(ext));
}

// =============================================================================
// Helper Functions
// =============================================================================

/**
 * Get line and column number from string index
 * @param content - Full content string
 * @param index - Character index
 */
function getLineInfo(
  content: string,
  index: number,
): { line: number; column: number } {
  const lines = content.substring(0, index).split("\n");
  const line = lines.length;
  const column = (lines[lines.length - 1]?.length ?? 0) + 1;
  return { line, column };
}

/**
 * Quick check if content might contain unsupported patterns
 * This is a fast pre-check before doing full regex matching
 * @param content - Source code content
 * @returns true if content might contain unsupported patterns
 */
export function mightContainUnsupportedPatterns(content: string): boolean {
  return QUICK_CHECK_PATTERNS.some((pattern) => content.includes(pattern));
}

/**
 * Get a summary of validation issues
 * @param result - Validation result
 */
export function getValidationSummary(result: ValidationResult): string {
  if (result.valid && result.warnings.length === 0) {
    return "No issues found";
  }

  const parts: string[] = [];

  if (result.errors.length > 0) {
    parts.push(`${result.errors.length} error(s)`);
  }

  if (result.warnings.length > 0) {
    parts.push(`${result.warnings.length} warning(s)`);
  }

  return parts.join(", ");
}
