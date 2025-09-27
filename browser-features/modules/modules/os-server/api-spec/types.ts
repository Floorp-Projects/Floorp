/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * TypeScript types for Floorp OS Local HTTP Server API
 * Generated from OpenAPI specification
 */

// Common Response Types
export interface OkResponse {
  ok: boolean;
}

export interface ErrorResponse {
  error: string;
}

export interface HealthResponse {
  status: string;
}

export interface Rect {
  x?: number;
  y?: number;
  width?: number;
  height?: number;
}

// Browser Information Types
export interface Tab {
  id: number;
  url: string;
  title: string;
  isActive: boolean;
  isPinned: boolean;
}

export interface HistoryItem {
  url: string;
  title: string;
  lastVisitDate: string; // epoch milliseconds as string
  visitCount: number;
}

export interface Download {
  id: number;
  url: string;
  filename: string;
  status: string;
  startTime: string; // epoch milliseconds as string
}

export interface BrowserContext {
  history: HistoryItem[];
  tabs: Tab[];
  downloads: Download[];
}

// Instance Management Types
export interface CreateInstanceResponse {
  instanceId: string;
}

export interface ExistsResponse {
  exists: boolean;
}

// Navigation Types
export interface NavigateRequest {
  url: string;
}

export interface URIResponse {
  uri?: string;
}

export interface TabURIResponse {
  uri: string;
}

export interface HTMLResponse {
  html?: string;
}

// Element Operations
export interface SelectorRequest {
  selector: string;
}

export interface TextResponse {
  text?: string;
}

export interface ValueResponse {
  value?: string;
}

export interface WaitForElementRequest {
  selector: string;
  timeout?: number;
}

export interface FoundResponse {
  found: boolean;
}

// Script Execution
export interface ExecuteScriptRequest {
  script: string;
}

// Screenshots
export interface ImageResponse {
  image?: string; // base64 encoded
}

export interface RegionScreenshotRequest {
  rect?: Rect;
}

// Form Operations
export interface FillFormRequest {
  formData: Record<string, string>; // CSS selector -> value mapping
}

// Tab Manager Specific Types
export interface TabEntry {
  browserId: string;
  title: string;
  url: string;
  selected: boolean;
  pinned: boolean;
}

export interface CreateTabInstanceRequest {
  url: string;
  inBackground?: boolean;
}

export interface AttachRequest {
  browserId: string;
}

export interface AttachResponse {
  instanceId?: string;
}
