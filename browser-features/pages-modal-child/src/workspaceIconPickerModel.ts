/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { TFormOption } from "../../chrome/common/modal-parent/utils/type.ts";

const CANONICAL_ICON_ID_PATTERN = /^floorp-icon:v1:[a-z0-9]+(?:-[a-z0-9]+)*$/;
const BUNDLED_SVG_DATA_URL_PREFIX = "data:image/svg+xml;base64,";
const BASE64_PAYLOAD_PATTERN = /^[A-Za-z0-9+/]+={0,2}$/;

export const isCanonicalWorkspaceIconPickerValue = (
  value: unknown,
): value is string =>
  typeof value === "string" && CANONICAL_ICON_ID_PATTERN.test(value);

export const isSafeWorkspaceIconPreview = (value: unknown): value is string => {
  if (
    typeof value !== "string" ||
    !value.startsWith(BUNDLED_SVG_DATA_URL_PREFIX)
  ) {
    return false;
  }
  return BASE64_PAYLOAD_PATTERN.test(
    value.slice(BUNDLED_SVG_DATA_URL_PREFIX.length),
  );
};

export const getSafeWorkspaceIconPickerOptions = (
  options: readonly TFormOption[],
): TFormOption[] => {
  const seen = new Set<string>();
  return options.filter((option) => {
    if (
      !isCanonicalWorkspaceIconPickerValue(option.value) ||
      !isSafeWorkspaceIconPreview(option.icon) ||
      seen.has(option.value)
    ) {
      return false;
    }
    seen.add(option.value);
    return true;
  });
};

const normalizeSearchTerm = (value: string): string =>
  value.trim().toLocaleLowerCase();

export const filterWorkspaceIconPickerOptions = (
  options: readonly TFormOption[],
  query: string,
): TFormOption[] => {
  const normalizedQuery = normalizeSearchTerm(query);
  if (!normalizedQuery) {
    return [...options];
  }

  return options.filter((option) => {
    const searchable = [
      option.label,
      option.value.slice("floorp-icon:v1:".length),
      ...(option.keywords ?? []),
    ];
    return searchable.some((value) =>
      normalizeSearchTerm(value).includes(normalizedQuery)
    );
  });
};

const hasOption = (
  options: readonly TFormOption[],
  value: unknown,
): value is string =>
  typeof value === "string" && options.some((option) => option.value === value);

export const resolveWorkspaceIconPickerDisplayValue = (
  formValue: unknown,
  initialDisplayValue: unknown,
  options: readonly TFormOption[],
): string | null => {
  if (hasOption(options, formValue)) {
    return formValue;
  }
  if (hasOption(options, initialDisplayValue)) {
    return initialDisplayValue;
  }
  return null;
};

export const selectWorkspaceIconPickerValue = (
  currentValue: string,
  candidate: unknown,
  options: readonly TFormOption[],
): string => hasOption(options, candidate) ? candidate : currentValue;
