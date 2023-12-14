/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useState, useRef, useCallback } from "react";

export const CopyButton = ({
  className,
  label,
  copiedLabel,
  inputSelector,
  transformer,
  ...props
}) => {
  const [copied, setCopied] = useState(false);
  const timeout = useRef(null);
  const onClick = useCallback(() => {
    let text = document.querySelector(inputSelector).value;
    if (transformer) {
      text = transformer(text);
    }
    navigator.clipboard.writeText(text);

    clearTimeout(timeout.current);
    setCopied(true);
    timeout.current = setTimeout(() => setCopied(false), 1500);
  }, [inputSelector, transformer]);
  return (
    <button className={className} onClick={e => onClick()} {...props}>
      {(copied && copiedLabel) || label}
    </button>
  );
};
