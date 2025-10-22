/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { Browser, Icon, Manifest } from "./type";
import { DataManager } from "./dataStore";
import { IconProcesser } from "./iconProcesser";
import { ManifestProcesser } from "./manifestProcesser";
import { createEffect, createRoot, createSignal } from "solid-js";

export class ProgressiveWebApp {
  private static instance: ProgressiveWebApp;
  private dataManager: DataManager;
  private iconProcesser: IconProcesser;
  private manifestProcesser: ManifestProcesser;
  private currentPWAs!: ReturnType<
    typeof createSignal<Record<string, Manifest>>
  >;

  private constructor() {
    this.dataManager = new DataManager();
    this.iconProcesser = new IconProcesser();
    this.manifestProcesser = new ManifestProcesser();
    createRoot(() => {
      this.currentPWAs = createSignal<Record<string, Manifest>>({});
      this.initializeSignals();
    });
  }

  public static getInstance(): ProgressiveWebApp {
    if (!ProgressiveWebApp.instance) {
      ProgressiveWebApp.instance = new ProgressiveWebApp();
    }
    return ProgressiveWebApp.instance;
  }

  private initializeSignals(): void {
    const [, setCurrentPWAs] = this.currentPWAs;
    createEffect(async () => {
      const data = await this.dataManager.getCurrentSsbData();
      setCurrentPWAs(data);
    });
  }

  /**
   * Register a new PWA/SSB from a browser instance
   * @param browser The browser instance containing the PWA information
   * @returns Promise<void>
   */
  public async registerFromBrowser(browser: Browser): Promise<void> {
    const manifest = await this.manifestProcesser.getManifestFromBrowser(
      browser,
      true,
    );

    if (!manifest) {
      return;
    }

    await this.dataManager.saveSsbData(manifest);
  }

  /**
   * Get all registered PWAs/SSBs
   * @returns Promise<Record<string, Manifest>>
   */
  public getAllPWAs(): Record<string, Manifest> {
    const [currentPWAs] = this.currentPWAs;
    return currentPWAs();
  }

  /**
   * Remove a PWA/SSB by its ID
   * @param id The ID of the PWA/SSB to remove
   * @returns Promise<void>
   */
  public async removePWA(id: string): Promise<void> {
    await this.dataManager.removeSsbData(id);
  }

  /**
   * Get the best icon for a PWA based on the desired size
   * @param icons Array of available icons
   * @param targetSize Desired icon size
   * @returns Icon The best matching icon
   */
  public getBestIcon(icons: Icon[], targetSize = 64): Icon {
    const iconList = this.iconProcesser.buildIconList(icons);
    let bestIcon = iconList[0]?.icon;

    for (const { icon, size } of iconList) {
      if (size >= targetSize) {
        bestIcon = icon;
        break;
      }
    }
    return bestIcon;
  }

  /**
   * Create a manifest for a PWA
   * @param browser The browser instance
   * @param useWebManifest Whether to use the web manifest or generate a new one
   * @returns Promise<Manifest>
   */
  public async createManifest(
    browser: Browser,
    useWebManifest: boolean,
  ): Promise<Manifest | undefined> {
    const result = await this.manifestProcesser.getManifestFromBrowser(
      browser,
      useWebManifest,
    );

    if (!result) {
      return;
    }

    return result;
  }

  /**
   * Check if a URL is already registered as a PWA
   * @param url The URL to check
   * @returns Promise<boolean>
   */
  public isPWARegistered(url: string): boolean {
    const [currentPWAs] = this.currentPWAs;
    return Object.values(currentPWAs()).some(
      (manifest) => manifest.start_url === url,
    );
  }

  /**
   * Get a PWA by its start URL
   * @param url The start URL of the PWA
   * @returns Promise<Manifest | null>
   */
  public getPWAByUrl(url: string): Manifest | null {
    const [currentPWAs] = this.currentPWAs;
    return (
      Object.values(currentPWAs()).find(
        (manifest) => manifest.start_url === url,
      ) || null
    );
  }

  /**
   * Get an SSB by its ID (which is the start_url)
   * @param id The ID (start_url) of the SSB to retrieve
   * @returns Promise<Manifest | null>
   */
  public getSsbById(id: string): Manifest | null {
    const [currentPWAs] = this.currentPWAs;
    const manifest = Object.values(currentPWAs()).find(
      (m) => m.start_url === id,
    );

    if (manifest) {
      return {
        name: manifest.name,
        id: manifest.start_url,
        start_url: manifest.start_url,
        icon: manifest.icon,
        short_name: manifest.short_name,
        scope: manifest.scope,
      };
    }
    return null;
  }
}
