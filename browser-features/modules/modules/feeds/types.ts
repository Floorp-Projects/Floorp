// SPDX-License-Identifier: MPL-2.0
import type { Article, SafeNode } from "#libs/newtab/types.ts";

export interface ParsedArticle {
  sourceKey: string;
  title: string;
  link: string | null;
  author: string;
  publishedAt: number | null;
  excerpt: string;
  hasFullContent: boolean;
  content: SafeNode[];
  images: Record<string, string>;
}
export interface ParsedFeed {
  title: string;
  siteUrl: string | null;
  articles: ParsedArticle[];
}
export interface StoredFeed {
  id: string;
  url: string;
  title: string;
  siteUrl: string | null;
  enabled: boolean;
  etag: string | null;
  lastModified: string | null;
  lastSuccessAt: number | null;
  nextAttemptAt: number;
  retryAfterAt: number | null;
  failureCount: number;
  error: string | null;
}
export interface FeedResponse {
  status: number;
  url: string;
  bytes: Uint8Array;
  headers: Record<string, string>;
  contentType: string;
  charset: string;
}
export interface StoredArticle extends Article {
  sourceKey: string;
  images: Record<string, string>;
}
