// SPDX-License-Identifier: MPL-2.0
import type { WallpaperProps } from "./types.ts";
import { useEffect, useRef, useState } from "react";
import { newTabClient } from "../../../../../libs/newtab/client.ts";
import { nextBackgroundDelay } from "./backgroundTiming.ts";
import {
  getRandomBackgroundImage,
  getSelectedFloorpImage,
} from "../../utils/backgroundImages.ts";

export function Wallpaper({
  settings,
  visible,
  onLoadedChange,
}: WallpaperProps) {
  const [image, setImage] = useState<string | null>(null);
  const [previous, setPrevious] = useState<string | null>(null);
  const current = useRef<string | null>(null);
  useEffect(() => {
    let active = true;
    let timer: ReturnType<typeof setTimeout> | undefined;
    let fade: ReturnType<typeof setTimeout> | undefined;
    let pending = false;
    const apply = async () => {
      if (pending) return;
      pending = true;
      try {
        let source: string | null = null;
        if (settings.type === "random") source = getRandomBackgroundImage();
        if (settings.type === "floorp")
          source = getSelectedFloorpImage(settings.selectedFloorp);
        if (settings.type === "custom") source = settings.customImage;
        if (settings.type === "folderPath" && settings.folderPath)
          source = (await newTabClient.call("folderImage", {})).image;
        if (!active) return;
        if (source === current.current) {
          setPrevious(null);
          return;
        }
        if (source)
          await new Promise<void>((resolve, reject) => {
            const candidate = new Image();
            candidate.onload = () => resolve();
            candidate.onerror = () =>
              reject(new Error("Background image unavailable"));
            candidate.src = source!;
          });
        if (!active) return;
        clearTimeout(fade);
        setPrevious(source ? current.current : null);
        current.current = source;
        setImage(source);
        onLoadedChange(Boolean(source));
        fade = setTimeout(() => {
          if (active) setPrevious(null);
        }, 1000);
      } catch {
        if (active && !current.current) {
          setImage(null);
          onLoadedChange(false);
        }
      } finally {
        pending = false;
      }
    };
    void apply();
    const initialDelay = nextBackgroundDelay(settings.slideshowInterval, 0);
    if (
      settings.slideshowEnabled &&
      ["random", "folderPath"].includes(settings.type) &&
      initialDelay !== null
    ) {
      let started = performance.now();
      const schedule = () => {
        if (!active) return;
        const delay = nextBackgroundDelay(
          settings.slideshowInterval,
          performance.now() - started,
        );
        if (delay === null) return;
        if (delay === 0) {
          started = performance.now();
          void apply();
        }
        timer = setTimeout(schedule, delay === 0 ? initialDelay : delay);
      };
      timer = setTimeout(schedule, initialDelay);
    }
    return () => {
      active = false;
      clearTimeout(timer);
      clearTimeout(fade);
    };
  }, [
    settings.type,
    settings.customImage,
    settings.folderPath,
    settings.selectedFloorp,
    settings.slideshowEnabled,
    settings.slideshowInterval,
    onLoadedChange,
  ]);
  return (
    <div
      className={`nt-wallpaper${visible ? "" : " is-hidden"}`}
      aria-hidden="true"
    >
      {previous && <img src={previous} alt="" />}
      {image && (
        <img className="nt-wallpaper-current" key={image} src={image} alt="" />
      )}
      <div className={image ? "nt-wallpaper-shade" : ""} />
    </div>
  );
}
