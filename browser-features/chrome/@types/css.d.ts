// SPDX-License-Identifier: MPL-2.0
// Augment CSSStyleDeclaration with commonly used CSS property accessors.
// The Gecko @types/gecko package omits these named properties, causing
// TS2339 errors throughout the chrome codebase.

/* eslint-disable @typescript-eslint/no-empty-object-interface */
interface CSSStyleDeclaration {
  backgroundColor: string;
  bottom: string;
  color: string;
  contentVisibility: string;
  display: string;
  filter: string;
  fontSize: string;
  fontWeight: string;
  height: string;
  left: string;
  opacity: string;
  overflow: string;
  padding: string;
  pointerEvents: string;
  position: string;
  top: string;
  transform: string;
  visibility: string;
  width: string;
  zIndex: string;
}
