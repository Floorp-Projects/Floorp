import React, { useEffect, useRef, useState } from "react";
import { Check, FolderOpen, ImagePlus } from "lucide-react";
import { Button } from "../../../../libs/ui/button.tsx";
import { Input } from "../../../../libs/ui/input.tsx";
import { Switch } from "../../../../libs/ui/switch.tsx";
import { prepareImageForStorage } from "../../../../browser-features/pages-newtab/src/utils/imageCompression.ts";
import {
  floorpWallpapers,
  randomWallpapers,
  saveFolderImages,
} from "./background-media.ts";
import { usePrototype } from "./store.tsx";
import type { BackgroundSettings as BackgroundPreferences } from "./types.ts";
import "./background-settings.css";

const backgroundModes: {
  value: BackgroundPreferences["type"];
  title: string;
  description: string;
}[] = [
  { value: "none", title: "背景なし", description: "テーマに合わせた単色" },
  {
    value: "random",
    title: "ランダムな画像",
    description: "内蔵の背景から自動で選ぶ",
  },
  { value: "custom", title: "自分の画像", description: "画像ファイルを選ぶ" },
  {
    value: "folderPath",
    title: "フォルダー内の画像",
    description: "選んだフォルダーから自動で選ぶ",
  },
  { value: "floorp", title: "Floorp の画像", description: "好きな1枚を選ぶ" },
];

export function BackgroundSettings() {
  const { data, setData, showToast } = usePrototype();
  const background = data.settings.background;
  const imageInput = useRef<HTMLInputElement>(null);
  const folderInput = useRef<HTMLInputElement>(null);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState("");

  useEffect(() => {
    folderInput.current?.setAttribute("webkitdirectory", "");
    folderInput.current?.setAttribute("directory", "");
  }, []);

  const updateBackground = (patch: Partial<BackgroundPreferences>) => {
    setData((current) => ({
      ...current,
      settings: {
        ...current.settings,
        background: { ...current.settings.background, ...patch },
      },
    }));
  };

  const chooseMode = (type: BackgroundPreferences["type"]) => {
    setError("");
    updateBackground({
      type,
      ...(type === "floorp" && !background.selectedFloorp
        ? { selectedFloorp: floorpWallpapers[0]?.name ?? null }
        : {}),
    });
  };

  const chooseImage = async (event: React.ChangeEvent<HTMLInputElement>) => {
    const file = event.currentTarget.files?.[0];
    event.currentTarget.value = "";
    if (!file) return;
    setBusy(true);
    setError("");
    try {
      const image = await prepareImageForStorage(file);
      if (image.length > 3_500_000) {
        setError(
          "画像を保存できませんでした。サイズの小さい画像を選んでください。",
        );
        return;
      }
      const decodedImage = new Image();
      decodedImage.src = image;
      await decodedImage.decode();
      updateBackground({
        type: "custom",
        customImage: image,
        fileName: file.name,
      });
      showToast("背景画像を変更しました");
    } catch {
      setError("画像を読み込めませんでした。別の画像を選んでください。");
    } finally {
      setBusy(false);
    }
  };

  const chooseFolder = async (event: React.ChangeEvent<HTMLInputElement>) => {
    const selectedFiles = Array.from(event.currentTarget.files ?? []);
    event.currentTarget.value = "";
    if (!selectedFiles.length) return;
    const files = selectedFiles.filter(
      (file) =>
        file.type.startsWith("image/") ||
        /\.(avif|png|jpe?g|webp|gif|bmp)$/i.test(file.name),
    );
    if (!files.length) {
      setError(
        "このフォルダーに画像がありません。別のフォルダーを選んでください。",
      );
      return;
    }
    setBusy(true);
    setError("");
    try {
      await saveFolderImages(files);
      updateBackground({
        type: "folderPath",
        folderPath:
          selectedFiles[0].webkitRelativePath.split("/")[0] ||
          "選択したフォルダー",
      });
      showToast(`${files.length}枚の画像を背景に設定しました`);
    } catch {
      setError("フォルダーを読み込めませんでした。もう一度選んでください。");
    } finally {
      setBusy(false);
    }
  };

  return (
    <div className="background-settings" aria-busy={busy}>
      <fieldset className="background-mode-options" disabled={busy}>
        <legend className="hub-sr-only">背景の種類</legend>
        {backgroundModes.map(({ value, title, description }) => (
          <label
            key={value}
            className="background-mode-option"
            data-selected={background.type === value}
          >
            <input
              type="radio"
              name="background-type"
              value={value}
              checked={background.type === value}
              onChange={() => chooseMode(value)}
            />
            <span>
              <strong>{title}</strong>
              <span>{description}</span>
            </span>
          </label>
        ))}
      </fieldset>

      <div className="hub-setting-row background-translucency-setting">
        <label htmlFor="background-translucency">
          <span
            id="background-translucency-label"
            className="hub-setting-label"
          >
            背景を透かす
          </span>
          <span
            id="background-translucency-description"
            className="hub-setting-hint"
          >
            パネルの背景をぼかして、壁紙がほのかに透ける表示にします。
          </span>
        </label>
        <Switch
          id="background-translucency"
          aria-labelledby="background-translucency-label"
          aria-describedby="background-translucency-description"
          checked={data.settings.translucentPanels}
          disabled={busy || background.type === "none"}
          onChange={(event) => {
            const checked = event.currentTarget.checked;
            setData((current) => ({
              ...current,
              settings: { ...current.settings, translucentPanels: checked },
            }));
          }}
        />
      </div>

      {background.type === "random" && (
        <div className="background-mode-detail">
          <div className="background-random-preview" aria-hidden="true">
            {randomWallpapers.slice(0, 3).map((image) => (
              <img key={image.name} src={image.url} alt="" />
            ))}
          </div>
          <p>新しいタブを開くたびに、背景が変わります。</p>
        </div>
      )}

      <input
        ref={imageInput}
        className="background-hidden-input"
        type="file"
        accept="image/*"
        tabIndex={-1}
        aria-hidden="true"
        onChange={chooseImage}
        disabled={busy}
      />
      {background.type === "custom" && (
        <div className="background-mode-detail background-custom-detail">
          {background.customImage && (
            <img
              className="background-current-preview"
              src={background.customImage}
              alt="選択中の背景"
            />
          )}
          <div>
            {background.fileName && (
              <p className="background-file-name">{background.fileName}</p>
            )}
            <Button
              variant="secondary"
              className="background-file-button"
              onClick={() => imageInput.current?.click()}
              disabled={busy}
            >
              <ImagePlus size={17} aria-hidden="true" />
              画像を選ぶ
            </Button>
          </div>
        </div>
      )}

      <input
        ref={folderInput}
        className="background-hidden-input"
        type="file"
        multiple
        tabIndex={-1}
        aria-hidden="true"
        onChange={chooseFolder}
        disabled={busy}
      />
      {background.type === "folderPath" && (
        <div className="background-mode-detail">
          {background.folderPath && (
            <p className="background-file-name">{background.folderPath}</p>
          )}
          <Button
            variant="secondary"
            className="background-file-button"
            onClick={() => folderInput.current?.click()}
            disabled={busy}
          >
            <FolderOpen size={17} aria-hidden="true" />
            フォルダーを選ぶ
          </Button>
        </div>
      )}

      {background.type === "floorp" && (
        <fieldset className="background-floorp-gallery" disabled={busy}>
          <legend className="hub-sr-only">Floorp の背景画像</legend>
          {floorpWallpapers.map((image) => (
            <label
              key={image.name}
              className="background-floorp-option"
              data-selected={background.selectedFloorp === image.name}
            >
              <input
                type="radio"
                name="floorp-wallpaper"
                value={image.name}
                checked={background.selectedFloorp === image.name}
                onChange={() =>
                  updateBackground({ selectedFloorp: image.name })
                }
              />
              <img src={image.url} alt="" />
              <span>{image.label}</span>
              {background.selectedFloorp === image.name && (
                <span className="background-selected-mark" aria-hidden="true">
                  <Check size={16} />
                </span>
              )}
            </label>
          ))}
        </fieldset>
      )}

      {(background.type === "random" || background.type === "folderPath") && (
        <div className="background-slideshow">
          <div className="hub-setting-row">
            <label htmlFor="background-slideshow">
              <span className="hub-setting-label">スライドショー</span>
              <span className="hub-setting-hint">
                背景を自動で切り替えます。
              </span>
            </label>
            <Switch
              id="background-slideshow"
              checked={background.slideshowEnabled}
              onChange={(event) =>
                updateBackground({
                  slideshowEnabled: event.currentTarget.checked,
                })
              }
              disabled={busy}
            />
          </div>
          {background.slideshowEnabled && (
            <div className="hub-setting-row">
              <label htmlFor="background-interval">
                <span className="hub-setting-label">切り替えの間隔</span>
              </label>
              <div className="background-interval-control">
                <Input
                  key={background.type}
                  id="background-interval"
                  className="background-interval-input"
                  type="number"
                  inputMode="numeric"
                  min={1}
                  step={1}
                  defaultValue={background.slideshowInterval}
                  onChange={(event) => {
                    const seconds = event.currentTarget.valueAsNumber;
                    if (Number.isFinite(seconds) && seconds >= 1) {
                      updateBackground({
                        slideshowInterval: Math.round(seconds),
                      });
                    }
                  }}
                  onBlur={(event) => {
                    event.currentTarget.value = String(
                      background.slideshowInterval,
                    );
                  }}
                  disabled={busy}
                />
                <span>秒</span>
              </div>
            </div>
          )}
        </div>
      )}
      {busy && <p className="background-loading">画像を読み込んでいます…</p>}
      {error && (
        <p className="background-error" role="alert">
          {error}
        </p>
      )}
    </div>
  );
}
