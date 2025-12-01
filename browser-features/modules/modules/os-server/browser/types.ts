/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Type definitions for BrowserInfo API
 */

export interface TabData {
  id: number;
  url: string;
  title: string;
  isActive: boolean;
  isPinned: boolean;
}

export interface HistoryData {
  url: string;
  title: string;
  lastVisitDate: number;
  visitCount: number;
}

export interface DownloadData {
  id: number;
  url: string;
  filename: string;
  status: string;
  startTime: number;
}

export interface BrowserInfoAPI {
  getRecentTabs(): Promise<TabData[]>;
  getRecentHistory(limit?: number): Promise<HistoryData[]>;
  getRecentDownloads(limit?: number): Promise<DownloadData[]>;
  getAllContextData(
    historyLimit?: number,
    downloadLimit?: number,
  ): Promise<{
    history: HistoryData[];
    tabs: TabData[];
    downloads: DownloadData[];
  }>;
}
