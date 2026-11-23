/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import styles from "@/components/common/settings-sections.module.css";
import {
  type ComponentProps,
  forwardRef,
  useEffect,
  useMemo,
  useRef,
  useState,
} from "react";
import { useTranslation } from "react-i18next";
import { ChevronDown, ChevronUp } from "lucide-react";
import { NumberInput } from "@chakra-ui/react";
import type { NumberFieldProps } from "../types.ts";

// Let React synchronize the native value in Gecko, including stepper changes.
// Ark's input handler also writes the value and restores a captured caret in a
// later frame, which can race React's controlled onChange. Keep its validation,
// keyboard and stepper handlers, but let React alone handle text input updates.
// Drop Ark's defaultValue because this input receives a controlled value.
const ControlledNumberInput = forwardRef<
  HTMLInputElement,
  ComponentProps<"input">
>(function ControlledNumberInput(
  { defaultValue: _defaultValue, onInput: _onInput, ...props },
  ref,
) {
  return <input {...props} ref={ref} />;
});

// Convert a draft back to a number with the same locale rules as the
// formatter: localized digits, then the localized decimal separator.
const DIGIT_ZEROES = [
  0x0030, // ASCII
  0x0660, // Arabic-Indic
  0x06f0, // Extended Arabic-Indic
  0x0966, // Devanagari
  0x0e50, // Thai
  0xff10, // Fullwidth
];
function parseLocalizedNumber(
  text: string,
  formatter: Intl.NumberFormat,
): number {
  const parts = formatter.formatToParts(1111.5);
  const group = parts.find((part) => part.type === "group")?.value;
  const decimals = new Set([
    parts.find((part) => part.type === "decimal")?.value ?? ".",
    ".",
    ",",
    "٫", // U+066B Arabic decimal separator
  ]);
  let normalized = "";
  for (const char of text.trim().normalize("NFKC")) {
    const code = char.codePointAt(0)!;
    const zero = DIGIT_ZEROES.find((z) => code >= z && code <= z + 9);
    if (zero !== undefined) {
      normalized += String(code - zero);
    } else if (decimals.has(char)) {
      normalized += ".";
    } else if (char !== group) {
      normalized += char;
    }
  }
  return Number(normalized);
}

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
  const { i18n } = useTranslation();
  const locale = i18n.language || "en-US";
  const formatOptions = useMemo<Intl.NumberFormatOptions>(() => ({
    useGrouping: false,
    maximumFractionDigits: 20,
  }), []);
  const formatter = useMemo(
    () => new Intl.NumberFormat(locale, formatOptions),
    [locale, formatOptions],
  );
  const [text, setTextState] = useState(() => formatter.format(value));
  // Zag's PRESS_UP commit can fire before React flushes the draft state, so
  // read the live text through a ref at commit time.
  const textRef = useRef(text);
  const setText = (next: string) => {
    textRef.current = next;
    setTextState(next);
  };
  const composing = useRef(false);
  // Ark's controlled-value watcher reformats and rewrites the DOM value on any
  // change, which drops a separately typed decimal separator and races IME
  // composition. Feed it only the committed value and let React own the draft.
  const committed = useMemo(
    () => formatter.format(value),
    [formatter, value],
  );

  useEffect(() => {
    setText(formatter.format(value));
  }, [value, formatter]);

  const commit = (parsed: number) => {
    if (composing.current) return;
    if (!Number.isFinite(parsed)) {
      setText(formatter.format(value));
      return;
    }

    const next = Math.max(min, Math.round(parsed));
    setText(formatter.format(next));
    if (next !== value) {
      onCommit(next);
    }
  };

  // Zag derives steps from the controlled value, which is pinned to the saved
  // pref — stepping would restart from it and ignore the visible draft. Step
  // from the parsed draft instead so repeated and post-edit steps accumulate.
  const stepBy = (amount: number) => {
    const parsed = textRef.current.trim() === ""
      ? 0
      : parseLocalizedNumber(textRef.current, formatter);
    const base = Number.isFinite(parsed) ? parsed : value;
    const next = Math.max(min, Math.round(base + amount));
    setText(formatter.format(next));
    if (next !== value) {
      onCommit(next);
    }
  };
  const onStepKey = (event: React.KeyboardEvent<HTMLInputElement>) => {
    if (event.key !== "ArrowUp" && event.key !== "ArrowDown") return;
    // During IME composition, arrows navigate the candidate list; do not
    // suppress the event. nativeEvent.isComposing covers keydowns that fire
    // before React delivers compositionstart, and keyCode 229 marks keys the
    // IME already consumed at composition boundaries (as Zag's guard does).
    if (
      composing.current || event.nativeEvent.isComposing ||
      event.keyCode === 229
    ) {
      return;
    }
    event.preventDefault();
    const step = event.shiftKey ? 10 : 1;
    stepBy(step * (event.key === "ArrowUp" ? 1 : -1));
  };

  return (
    <div className={styles.field}>
      <label htmlFor={id} className="text-sm">
        {label}
      </label>
      <NumberInput.Root
        ids={{ input: id }}
        min={min}
        locale={locale}
        // Use the same locale parser for validation and numeric conversion.
        // Without formatOptions, Ark accepts localized digits but then parses
        // them with parseFloat, which can erase or truncate the draft.
        formatOptions={formatOptions}
        value={committed}
        disabled={disabled}
        colorPalette="purple"
        onValueChange={({ value }) => {
          if (!composing.current) setText(value);
        }}
        onValueCommit={() =>
          commit(
            textRef.current.trim() === ""
              ? 0
              : parseLocalizedNumber(textRef.current, formatter),
          )}
      >
        <NumberInput.Input asChild>
          <ControlledNumberInput
            value={text}
            onKeyDown={onStepKey}
            onChange={(event) => setText(event.currentTarget.value)}
            onCompositionStart={() => {
              composing.current = true;
            }}
            onCompositionEnd={(event) => {
              composing.current = false;
              setText(event.currentTarget.value);
            }}
          />
        </NumberInput.Input>
        {/* Zag derives trigger presses from the pinned controlled value, so
            its triggers are not used; these plain buttons step from the draft
            and keep the recipe styling via data-part. */}
        <NumberInput.Control>
          <button
            type="button"
            tabIndex={-1}
            aria-label="increment"
            className="chakra-number-input__incrementTrigger"
            data-part="increment-trigger"
            disabled={disabled}
            // Keep the input focused like the Zag triggers did, so a mouse
            // press does not blur-commit and then step again.
            onMouseDown={(event) => event.preventDefault()}
            onClick={() => stepBy(1)}
          >
            <ChevronUp />
          </button>
          <button
            type="button"
            tabIndex={-1}
            aria-label="decrement"
            className="chakra-number-input__decrementTrigger"
            data-part="decrement-trigger"
            disabled={disabled}
            onMouseDown={(event) => event.preventDefault()}
            onClick={() => stepBy(-1)}
          >
            <ChevronDown />
          </button>
        </NumberInput.Control>
      </NumberInput.Root>
    </div>
  );
}
