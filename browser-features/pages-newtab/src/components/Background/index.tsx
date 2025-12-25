import { useBackground } from "@/contexts/BackgroundContext.tsx";
import { useEffect, useRef, useState } from "react";
import {
  getRandomBackgroundImage,
  getSelectedFloorpImage,
} from "../../utils/backgroundImages.ts";
import { getRandomImageFromFolder } from "../../utils/dataManager.ts";

export function Background() {
  const {
    type,
    customImage,
    folderPath,
    selectedFloorp,
    slideshowEnabled,
    slideshowInterval,
  } = useBackground();
  // no debug logs in production
  const [currentImage, setCurrentImage] = useState<string | null>(null);
  const [nextImage, setNextImage] = useState<string | null>(null);
  const [isFading, setIsFading] = useState(false);
  const isTransitioningRef = useRef(false);

  const getNextImage = async (): Promise<string | null> => {
    if (type === "random") {
      const img = getRandomBackgroundImage();
      return img;
    }
    if (type === "folderPath" && folderPath) {
      const result = await getRandomImageFromFolder(folderPath);
      return result.success ? result.image : null;
    }
    return null;
  };

  useEffect(() => {
    const initialize = async () => {
      //
      if (slideshowEnabled && (type === "random" || type === "folderPath")) {
        // In slideshow mode, let the slideshow effect handle the first image.
        if (!currentImage) {
          const newImage = await getNextImage();
          setCurrentImage(newImage);
        }
        return;
      }

      if (type === "random") {
        const img = getRandomBackgroundImage();
        if (img) setCurrentImage(img);
      } else if (type === "folderPath" && folderPath) {
        const result = await getRandomImageFromFolder(folderPath);
        if (result.success && result.image) {
          setCurrentImage(result.image);
        } else {
          console.error(
            "Failed to load initial image from folder or no images found",
          );
          setCurrentImage(null);
        }
      } else if (type === "custom" && customImage) {
        setCurrentImage(customImage);
      } else if (type === "floorp" && selectedFloorp) {
        setCurrentImage(getSelectedFloorpImage(selectedFloorp));
      } else {
        setCurrentImage(null);
      }
    };
    initialize();
  }, [type, folderPath, customImage, selectedFloorp, slideshowEnabled]);

  useEffect(() => {
    // no debug logs here
    let timeoutId: ReturnType<typeof setTimeout> | undefined;

    if (
      slideshowEnabled &&
      (type === "random" || type === "folderPath") &&
      slideshowInterval > 0
    ) {
      const intervalId = setInterval(async () => {
        if (isTransitioningRef.current) {
          return;
        }
        isTransitioningRef.current = true;

        const newImage = await getNextImage();
        if (newImage && newImage !== currentImage) {
          // start fading to nextImage
          setIsFading(true);
          setNextImage(newImage);

          timeoutId = setTimeout(() => {
            setIsFading(false);
            setCurrentImage(newImage);
            setNextImage(null);
            isTransitioningRef.current = false;
          }, 1000); // Must match transition duration
        } else {
          isTransitioningRef.current = false;
        }
      }, slideshowInterval * 1000);

      return () => {
        clearInterval(intervalId);
        if (timeoutId) {
          clearTimeout(timeoutId);
        }
      };
    }
  }, [slideshowEnabled, type, slideshowInterval, folderPath, currentImage]);

  const transitionClass = isFading ? "transition-opacity duration-1000" : "";
  // Render two stacked layers so we can cross-fade between current and next images.
  // This fixes the issue where the single element hides itself while the next image
  // wasn't applied yet, resulting in a blank background.
  return (
    <div className="fixed inset-0">
      {/* Current image layer (fades out when nextImage is present) */}
      <div
        className={`absolute inset-0 bg-cover bg-center ${transitionClass}`}
        style={{
          backgroundImage: currentImage ? `url("${currentImage}")` : "none",
          opacity: nextImage ? 0 : 1,
        }}
      />

      {/* Next image layer (fades in) */}
      <div
        className={`absolute inset-0 bg-cover bg-center ${transitionClass}`}
        style={{
          backgroundImage: nextImage ? `url("${nextImage}")` : "none",
          opacity: nextImage ? 1 : 0,
        }}
      />
    </div>
  );
}
