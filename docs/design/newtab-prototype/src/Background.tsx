import React, { useEffect, useRef, useState } from "react";
import { FOLDER_IMAGES_CHANGED, floorpWallpapers, randomWallpapers, readFolderImages } from "./background-media.ts";
import { usePrototype } from "./store.tsx";

export function NewTabBackground({ onImageChange }: { onImageChange: (visible: boolean) => void }) {
  const { data } = usePrototype();
  const background = data.settings.background;
  const [image, setImage] = useState<string | null>(null);
  const [previous, setPrevious] = useState<string | null>(null);
  const [revision, setRevision] = useState(0);
  const current = useRef<string | null>(null);

  useEffect(() => {
    const reload = () => setRevision((value) => value + 1);
    window.addEventListener(FOLDER_IMAGES_CHANGED, reload);
    return () => window.removeEventListener(FOLDER_IMAGES_CHANGED, reload);
  }, []);

  useEffect(() => {
    let cancelled = false;
    let urls: string[] = [];
    let interval: ReturnType<typeof setInterval> | undefined;
    let fade: ReturnType<typeof setTimeout> | undefined;
    let pending: HTMLImageElement | undefined;
    const display = (url: string | null) => {
      if (cancelled || current.current === url) return;
      clearTimeout(fade);
      setPrevious(current.current);
      current.current = url;
      setImage(url);
      onImageChange(Boolean(url));
      fade = setTimeout(() => setPrevious(null), 1000);
    };
    const load = (url: string | null) => {
      if (!url) return display(null);
      pending = new Image();
      pending.onload = () => display(url);
      pending.onerror = () => display(null);
      pending.src = url;
    };
    const initialize = async () => {
      let candidates: string[] = [];
      try {
        if (background.type === "random") candidates = randomWallpapers.map((item) => item.url);
        if (background.type === "floorp") candidates = floorpWallpapers
          .filter((item) => item.name === background.selectedFloorp).map((item) => item.url);
        if (background.type === "custom" && background.customImage) candidates = [background.customImage];
        if (background.type === "folderPath" && background.folderPath) {
          const files = await readFolderImages();
          if (cancelled) return;
          urls = files.map((file) => URL.createObjectURL(file));
          candidates = urls;
        }
      } catch {
        // A missing local preview folder falls back to the normal page surface.
      }
      if (cancelled) return;
      const choose = () => {
        const alternatives = candidates.filter((url) => url !== current.current);
        const pool = alternatives.length ? alternatives : candidates;
        load(pool[Math.floor(Math.random() * pool.length)] ?? null);
      };
      choose();
      if (background.slideshowEnabled && (background.type === "random" || background.type === "folderPath")) {
        interval = setInterval(choose, Math.max(1, background.slideshowInterval) * 1000);
      }
    };
    void initialize();
    return () => {
      cancelled = true;
      clearInterval(interval);
      clearTimeout(fade);
      if (pending) { pending.onload = null; pending.onerror = null; }
      urls.forEach((url) => URL.revokeObjectURL(url));
    };
  }, [background.type, background.customImage, background.selectedFloorp, background.folderPath, background.slideshowEnabled, background.slideshowInterval, revision, onImageChange]);

  return (
    <div className="newtab-background" aria-hidden="true">
      {previous && <img className="wallpaper-previous" src={previous} alt="" />}
      {image && <img className="wallpaper-current" key={image} src={image} alt="" />}
      <div className="wallpaper-shade" />
    </div>
  );
}
