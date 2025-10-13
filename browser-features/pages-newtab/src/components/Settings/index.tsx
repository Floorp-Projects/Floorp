import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import { Modal } from "../Modal/index.tsx";
import { useBackground } from "@/contexts/BackgroundContext.tsx";
import { useComponents } from "@/contexts/ComponentsContext.tsx";
import { getFloorpImages } from "@/utils/backgroundImages.ts";
import {
  getFolderPathFromDialog,
  getNewTabSettings,
  saveNewTabSettings,
} from "@/utils/dataManager.ts";
import {
  getDisableFloorpStart,
  setDisableFloorpStart,
} from "@/utils/designPref.ts";

export function Settings({
  isOpen,
  onClose,
}: { isOpen: boolean; onClose: () => void }) {
  const { t } = useTranslation();
  const {
    type: backgroundType,
    fileName,
    folderPath,
    selectedFloorp,
    setType: setBackgroundType,
    setCustomImage,
    setFolderPath,
    setSelectedFloorp,
    slideshowEnabled,
    slideshowInterval,
    setSlideshowEnabled,
    setSlideshowInterval,
  } = useBackground();
  const { components, toggleComponent } = useComponents();
  const [isSubmitting, setIsSubmitting] = useState(false);
  const [currentFileName, setCurrentFileName] = useState<string>("");
  const [currentFolderPath, setCurrentFolderPath] = useState<string>("");
  const [floorpImages, setFloorpImages] = useState<
    { name: string; url: string }[]
  >([]);
  const [blockedSites, setBlockedSites] = useState<string[]>([]);
  const [disableFloorpStart, setDisableFloorpStartState] = useState(false);

  useEffect(() => {
    if (backgroundType === "custom" && fileName) {
      setCurrentFileName(fileName);
    } else {
      setCurrentFileName("");
    }

    if (backgroundType === "folderPath" && folderPath) {
      setCurrentFolderPath(folderPath);
    } else {
      setCurrentFolderPath("");
    }

    setFloorpImages(getFloorpImages());
  }, [backgroundType, fileName, folderPath]);

  useEffect(() => {
    const loadBlockedSites = async () => {
      const settings = await getNewTabSettings();
      setBlockedSites(settings.topSites.blocked);
    };
    const loadDisableFloorpStart = async () => {
      const value = await getDisableFloorpStart();
      setDisableFloorpStartState(value);
    };

    if (isOpen) {
      loadBlockedSites();
      loadDisableFloorpStart();
    }
  }, [isOpen]);

  const compressImage = (
    dataUrl: string,
    maxWidth = 1920,
    maxHeight = 1080,
    quality = 0.8,
  ): Promise<string> => {
    return new Promise((resolve, reject) => {
      const img = new Image();
      img.onload = () => {
        const canvas = document.createElement("canvas");
        const ctx = canvas.getContext("2d");
        if (!ctx) {
          reject(new Error("Canvas context not available"));
          return;
        }

        // Calculate new dimensions
        let { width, height } = img;
        if (width > maxWidth) {
          height = (height * maxWidth) / width;
          width = maxWidth;
        }
        if (height > maxHeight) {
          width = (width * maxHeight) / height;
          height = maxHeight;
        }

        canvas.width = width;
        canvas.height = height;

        // Draw and compress
        ctx.drawImage(img, 0, 0, width, height);
        const compressedDataUrl = canvas.toDataURL("image/jpeg", quality);
        resolve(compressedDataUrl);
      };
      img.onerror = reject;
      img.src = dataUrl;
    });
  };

  const handleFileChange = async (
    event: React.ChangeEvent<HTMLInputElement>,
  ) => {
    const file = event.target.files?.[0];
    if (!file) return;

    setIsSubmitting(true);
    try {
      const reader = new FileReader();
      const imageData = await new Promise<string>((resolve, reject) => {
        reader.onload = () => resolve(reader.result as string);
        reader.onerror = reject;
        reader.readAsDataURL(file);
      });

      // Compress image if it's too large
      let finalImageData = imageData;
      if (imageData.length > 500000) {
        finalImageData = await compressImage(imageData, 1920, 1080, 0.8);
      }

      // Final size check
      if (finalImageData.length > 1000000) {
        return;
      }

      await setCustomImage(finalImageData, file.name);
      setCurrentFileName(file.name);
    } catch (error) {
      console.error("Failed to load image:", error);
    } finally {
      setIsSubmitting(false);
    }
  };

  const handleFolderSelect = async () => {
    setIsSubmitting(true);
    try {
      const folderPath = await getFolderPathFromDialog();
      if (folderPath) {
        await setFolderPath(folderPath.path);
      }
    } finally {
      setIsSubmitting(false);
    }
  };

  const handleFloorpImageSelect = async (imageName: string) => {
    setIsSubmitting(true);
    try {
      await setSelectedFloorp(imageName);
    } catch (error) {
      console.error("Failed to select Floorp image:", error);
    } finally {
      setIsSubmitting(false);
    }
  };

  const handleBackgroundTypeChange = async (type: typeof backgroundType) => {
    setIsSubmitting(true);
    try {
      if (type === "none") {
        await setCustomImage(null, null);
      }
      // Don't clear custom image data when switching to custom type
      // Only change the type, preserve existing image data
      await setBackgroundType(type);
    } catch (error) {
      console.error("Failed to change background type:", error);
    } finally {
      setIsSubmitting(false);
    }
  };

  const unblockSite = async (urlToUnblock: string) => {
    setIsSubmitting(true);
    try {
      const settings = await getNewTabSettings();
      const newBlockedSites = settings.topSites.blocked.filter(
        (url) => url !== urlToUnblock,
      );
      await saveNewTabSettings({
        topSites: {
          ...settings.topSites,
          blocked: newBlockedSites,
        },
      });
      setBlockedSites(newBlockedSites);
    } catch (error) {
      console.error("Failed to unblock site:", error);
    } finally {
      setIsSubmitting(false);
    }
  };

  return (
    <Modal
      isOpen={isOpen}
      onClose={onClose}
      title={t("settings.newTabSettings")}
    >
      <div className="space-y-6">
        <section>
          <h3 className="text-lg font-medium text-gray-900 dark:text-white mb-4">
            {t("settings.displayComponents")}
          </h3>
          <div className="space-y-4">
            <label className="flex items-center space-x-3">
              <input
                type="checkbox"
                checked={components.topSites}
                onChange={() => toggleComponent("topSites")}
                disabled={isSubmitting}
                className="form-checkbox h-5 w-5 text-primary rounded border-gray-300 dark:border-gray-600 focus:ring-primary"
              />
              <span className="text-gray-700 dark:text-gray-200">
                {t("settings.topSites")}
              </span>
            </label>
            <label className="flex items-center space-x-3">
              <input
                type="checkbox"
                checked={components.clock}
                onChange={() => toggleComponent("clock")}
                disabled={isSubmitting}
                className="form-checkbox h-5 w-5 text-primary rounded border-gray-300 dark:border-gray-600 focus:ring-primary"
              />
              <span className="text-gray-700 dark:text-gray-200">
                {t("settings.clock")}
              </span>
            </label>
            <label className="flex items-center space-x-3">
              <input
                type="checkbox"
                checked={components.searchBar}
                onChange={() => toggleComponent("searchBar")}
                disabled={isSubmitting}
                className="form-checkbox h-5 w-5 text-primary rounded border-gray-300 dark:border-gray-600 focus:ring-primary"
              />
              <span className="text-gray-700 dark:text-gray-200">
                {t("settings.searchBar")}
              </span>
            </label>
            <label className="flex items-center space-x-3">
              <input
                type="checkbox"
                checked={components.firefoxLayout}
                onChange={() => toggleComponent("firefoxLayout")}
                disabled={isSubmitting}
                className="form-checkbox h-5 w-5 text-primary rounded border-gray-300 dark:border-gray-600 focus:ring-primary"
              />
              <span className="text-gray-700 dark:text-gray-200">
                {t("settings.firefoxLayout")}
              </span>
            </label>
          </div>
        </section>

        <section>
          <h3 className="text-lg font-medium text-gray-900 dark:text-white mb-4">
            {t("settings.floorpStart")}
          </h3>
          <div className="space-y-2">
            <label className="flex items-start space-x-3">
              <input
                type="checkbox"
                checked={disableFloorpStart}
                onChange={async (e) => {
                  const v = e.target.checked;
                  setIsSubmitting(true);
                  try {
                    setDisableFloorpStartState(v);
                    await setDisableFloorpStart(v);
                  } finally {
                    setIsSubmitting(false);
                  }
                }}
                disabled={isSubmitting}
                className="form-checkbox h-5 w-5 text-primary rounded border-gray-300 dark:border-gray-600 focus:ring-primary mt-1"
              />
              <span className="text-gray-700 dark:text-gray-200">
                <span className="block font-medium">
                  {t("settings.disableFloorpStart")}
                </span>
                <span className="block text-sm text-gray-500 dark:text-gray-400">
                  {t("settings.disableFloorpStartDescription")}
                </span>
                <span className="block text-xs text-warning dark:text-yellow-400 mt-1">
                  {t("settings.restartRequired")}
                </span>
              </span>
            </label>
          </div>
        </section>

        <section>
          <h3 className="text-lg font-medium text-gray-900 dark:text-white mb-4">
            {t("settings.blockedSites", "Blocked Sites")}
          </h3>
          <div className="space-y-2 max-h-48 overflow-y-auto pr-2">
            {blockedSites.length > 0
              ? (
                blockedSites.map((url) => (
                  <div
                    key={url}
                    className="flex items-center justify-between bg-gray-100 dark:bg-gray-700/50 p-2 rounded-md"
                  >
                    <p
                      className="text-sm text-gray-700 dark:text-gray-200 truncate"
                      title={url}
                    >
                      {url}
                    </p>
                    <button
                      type="button"
                      onClick={() => unblockSite(url)}
                      disabled={isSubmitting}
                      className="btn btn-sm btn-ghost text-primary hover:bg-primary/10"
                    >
                      {t("settings.unblock", "Unblock")}
                    </button>
                  </div>
                ))
              )
              : (
                <p className="text-sm text-gray-500 dark:text-gray-400">
                  {t("settings.noBlockedSites", "No sites are blocked.")}
                </p>
              )}
          </div>
        </section>

        <section>
          <h3 className="text-lg font-medium text-gray-900 dark:text-white mb-4">
            {t("settings.backgroundSettings")}
          </h3>
          <div className="space-y-4">
            <label className="flex items-center space-x-3">
              <input
                type="radio"
                name="background"
                value="none"
                checked={backgroundType === "none"}
                onChange={() => handleBackgroundTypeChange("none")}
                disabled={isSubmitting}
                className="form-radio h-5 w-5 text-primary border-gray-300 dark:border-gray-600 focus:ring-primary"
              />
              <span className="text-gray-700 dark:text-gray-200">
                {t("settings.noBackground")}
              </span>
            </label>
            <label className="flex items-center space-x-3">
              <input
                type="radio"
                name="background"
                value="random"
                checked={backgroundType === "random"}
                onChange={() => handleBackgroundTypeChange("random")}
                disabled={isSubmitting}
                className="form-radio h-5 w-5 text-primary border-gray-300 dark:border-gray-600 focus:ring-primary"
              />
              <span className="text-gray-700 dark:text-gray-200">
                {t("settings.randomImage")}
              </span>
            </label>
            <label className="flex items-center space-x-3">
              <input
                type="radio"
                name="background"
                value="custom"
                checked={backgroundType === "custom"}
                onChange={() => handleBackgroundTypeChange("custom")}
                disabled={isSubmitting}
                className="form-radio h-5 w-5 text-primary border-gray-300 dark:border-gray-600 focus:ring-primary"
              />
              <span className="text-gray-700 dark:text-gray-200">
                {t("settings.customImage")}
              </span>
            </label>

            <label className="flex items-center space-x-3">
              <input
                type="radio"
                name="background"
                value="folderPath"
                checked={backgroundType === "folderPath"}
                onChange={() => handleBackgroundTypeChange("folderPath")}
                disabled={isSubmitting}
                className="form-radio h-5 w-5 text-primary border-gray-300 dark:border-gray-600 focus:ring-primary"
              />
              <span className="text-gray-700 dark:text-gray-200">
                {t("settings.folderImages")}
              </span>
            </label>

            <label className="flex items-center space-x-3">
              <input
                type="radio"
                name="background"
                value="floorp"
                checked={backgroundType === "floorp"}
                onChange={() => handleBackgroundTypeChange("floorp")}
                disabled={isSubmitting}
                className="form-radio h-5 w-5 text-primary border-gray-300 dark:border-gray-600 focus:ring-primary"
              />
              <span className="text-gray-700 dark:text-gray-200">
                {t("settings.floorpImages")}
              </span>
            </label>

            {backgroundType === "custom" && (
              <div className="mt-4 pl-8">
                {currentFileName && (
                  <div className="mb-2 text-sm text-gray-600 dark:text-gray-400">
                    {t("settings.currentImage")} {currentFileName}
                  </div>
                )}
                <input
                  type="file"
                  accept="image/*"
                  onChange={handleFileChange}
                  disabled={isSubmitting}
                  className="file-input block w-full text-sm text-gray-500 dark:text-gray-400
                    file:mr-4 file:py-2 file:px-4
                    file:rounded-full file:border-0
                    file:text-sm file:font-semibold
                    file:bg-primary/10 file:text-primary
                    hover:file:bg-primary/20
                    file:cursor-pointer"
                />
                <p className="mt-2 text-sm text-gray-500 dark:text-gray-400">
                  {t("settings.imageRecommendation")}
                </p>
              </div>
            )}

            {backgroundType === "folderPath" && (
              <div className="mt-4 pl-8">
                {currentFolderPath && (
                  <div className="mb-2 text-sm text-gray-600 dark:text-gray-400">
                    {t("settings.currentFolder")} {currentFolderPath}
                  </div>
                )}
                <button
                  type="button"
                  onClick={handleFolderSelect}
                  disabled={isSubmitting}
                  className="px-4 py-2 bg-primary/10 text-primary rounded-full text-sm font-semibold hover:bg-primary/20 transition-colors"
                >
                  {t("settings.selectFolder")}
                </button>
              </div>
            )}

            {(backgroundType === "random" ||
              backgroundType === "folderPath") && (
              <div className="mt-4 pl-8 space-y-4">
                <label className="flex items-center space-x-3">
                  <input
                    type="checkbox"
                    checked={slideshowEnabled}
                    onChange={(e) => setSlideshowEnabled(e.target.checked)}
                    disabled={isSubmitting}
                    className="form-checkbox h-5 w-5 text-primary rounded border-gray-300 dark:border-gray-600 focus:ring-primary"
                  />
                  <span className="text-gray-700 dark:text-gray-200">
                    {t("settings.enableSlideshow")}
                  </span>
                </label>
                {slideshowEnabled && (
                  <label className="flex items-center space-x-3">
                    <input
                      type="number"
                      value={slideshowInterval}
                      onChange={(e) =>
                        setSlideshowInterval(Number(e.target.value))}
                      disabled={isSubmitting}
                      className="form-input w-24 rounded-md border-gray-300 shadow-sm focus:border-primary focus:ring focus:ring-primary focus:ring-opacity-50 dark:bg-gray-700 dark:border-gray-600 dark:text-white"
                      min="1"
                    />
                    <span className="text-gray-700 dark:text-gray-200">
                      {t("settings.slideshowInterval")}
                    </span>
                  </label>
                )}
              </div>
            )}

            {backgroundType === "floorp" && (
              <div className="mt-4 pl-8">
                <div className="grid grid-cols-3 gap-4">
                  {floorpImages.map((image) => (
                    <div
                      key={image.name}
                      className={`
                        relative cursor-pointer rounded-lg overflow-hidden border-2
                        ${
                        selectedFloorp === image.name
                          ? "border-primary"
                          : "border-transparent"
                      }
                      `}
                      onClick={() => handleFloorpImageSelect(image.name)}
                    >
                      <img
                        src={image.url}
                        alt={image.name}
                        className="w-full h-auto aspect-video object-cover"
                      />
                      {selectedFloorp === image.name && (
                        <div className="absolute inset-0 bg-primary/20 flex items-center justify-center">
                          <span className="bg-primary text-white px-2 py-1 rounded text-xs">
                            {t("settings.selected")}
                          </span>
                        </div>
                      )}
                    </div>
                  ))}
                </div>
              </div>
            )}
          </div>
        </section>
      </div>
    </Modal>
  );
}
