import mountain from "../../../../browser-features/pages-newtab/src/assets/background/newtabbg-03.avif?inline";
import hills from "../../../../browser-features/pages-newtab/src/assets/background/newtabbg-04.avif?inline";
import forest from "../../../../browser-features/pages-newtab/src/assets/background/newtabbg-06.avif?inline";
import dawn from "../../../../browser-features/pages-newtab/src/assets/floorp/0400-0600.png?inline";
import morning from "../../../../browser-features/pages-newtab/src/assets/floorp/0600-1100.png?inline";
import day from "../../../../browser-features/pages-newtab/src/assets/floorp/1100-1600.png?inline";
import evening from "../../../../browser-features/pages-newtab/src/assets/floorp/1600-1800.png?inline";
import night from "../../../../browser-features/pages-newtab/src/assets/floorp/1800-2300.png?inline";
import midnight from "../../../../browser-features/pages-newtab/src/assets/floorp/2300-0400.png?inline";
import type { WallpaperAsset } from "./types.ts";

// A small sample keeps the standalone design preview portable. The product
// continues to use the complete existing background collection.
export const randomWallpapers: WallpaperAsset[] = [
  { name: "newtabbg-03.avif", label: "山並み", url: mountain },
  { name: "newtabbg-04.avif", label: "丘陵", url: hills },
  { name: "newtabbg-06.avif", label: "森", url: forest },
];

export const floorpWallpapers: WallpaperAsset[] = [
  { name: "0400-0600.png", label: "夜明け", url: dawn },
  { name: "0600-1100.png", label: "朝", url: morning },
  { name: "1100-1600.png", label: "昼", url: day },
  { name: "1600-1800.png", label: "夕方", url: evening },
  { name: "1800-2300.png", label: "夜", url: night },
  { name: "2300-0400.png", label: "深夜", url: midnight },
];

export const FOLDER_IMAGES_CHANGED = "floorp-prototype-folder-images-changed";

function openMediaStore(): Promise<IDBDatabase> {
  return new Promise((resolve, reject) => {
    const request = indexedDB.open("floorp-newtab-design-media", 1);
    request.onupgradeneeded = () => {
      request.result.createObjectStore("backgrounds");
    };
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => reject(request.error);
  });
}

export async function saveFolderImages(files: File[]): Promise<void> {
  const database = await openMediaStore();
  try {
    await new Promise<void>((resolve, reject) => {
      const transaction = database.transaction("backgrounds", "readwrite");
      transaction.objectStore("backgrounds").put(files, "folder");
      transaction.oncomplete = () => resolve();
      transaction.onerror = () => reject(transaction.error);
      transaction.onabort = () => reject(transaction.error);
    });
    window.dispatchEvent(new Event(FOLDER_IMAGES_CHANGED));
  } finally {
    database.close();
  }
}

export async function readFolderImages(): Promise<Blob[]> {
  const database = await openMediaStore();
  try {
    return await new Promise((resolve, reject) => {
      const request = database.transaction("backgrounds", "readonly")
        .objectStore("backgrounds").get("folder");
      request.onsuccess = () => {
        const result: unknown = request.result;
        resolve(Array.isArray(result) ? result.filter((item): item is Blob => item instanceof Blob) : []);
      };
      request.onerror = () => reject(request.error);
    });
  } finally {
    database.close();
  }
}
