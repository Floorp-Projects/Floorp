/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import styles from "@/components/common/settings-sections.module.css";
import { type ComponentProps, forwardRef, useEffect, useState } from "react";
import { NumberInput } from "@chakra-ui/react";
import type { NumberFieldProps } from "../types.ts";

/** Forwards the controlled value to the native input. */
// Let React synchronize the native value in Gecko, including stepper changes.
// Remove Ark's defaultValue when supplying a controlled value through asChild.
const ControlledNumberInput = forwardRef<
  HTMLInputElement,
  ComponentProps<"input">
>(function ControlledNumberInput(
  { defaultValue: _defaultValue, ...props },
  ref,
) {
  return <input {...props} ref={ref} />;
});

/**
 * Number input that keeps the raw text while typing, so a half-typed or
 * cleared field does not immediately write a clamped value to the pref.
 */
export function NumberField({
  id,
  label,
  value,
  min,
  disabled,
  onCommit,
}: NumberFieldProps) {
  const [text, setText] = useState(String(value));

  useEffect(() => {
    setText(String(value));
  }, [value]);

  /** Writes the rounded, clamped value through `onCommit` when it changed. */
  const commit = (rawValue: string) => {
    const parsed = Number(rawValue);
    if (!Number.isFinite(parsed)) {
      setText(String(value));
      return;
    }

    const next = Math.max(min, Math.round(parsed));
    setText(String(next));
    if (next !== value) {
      onCommit(next);
    }
  };

  return (
    <div className={styles.field}>
      <label htmlFor={id} className="text-sm">
        {label}
      </label>
      <NumberInput.Root
        ids={{ input: id }}
        min={min}
        value={text}
        disabled={disabled}
        colorPalette="purple"
        onValueChange={({ value }) => setText(value)}
        onValueCommit={({ value }) => commit(value)}
      >
        <NumberInput.Input asChild>
          <ControlledNumberInput
            value={text}
            onChange={(event) => setText(event.currentTarget.value)}
          />
        </NumberInput.Input>
        <NumberInput.Control />
      </NumberInput.Root>
    </div>
  );
}
