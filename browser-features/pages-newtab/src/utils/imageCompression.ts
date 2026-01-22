const BASE64_DATA_INDEX = 1;

interface ImageCompressionOptions {
  maxBytes: number;
  initialQuality: number;
  minQuality: number;
  qualityStep: number;
  resizeStep: number;
  minWidth: number;
  minHeight: number;
  outputType: "image/webp";
}

const DEFAULT_OPTIONS: ImageCompressionOptions = {
  maxBytes: 2_500_000,
  initialQuality: 0.9,
  minQuality: 0.5,
  qualityStep: 0.1,
  resizeStep: 0.85,
  minWidth: 720,
  minHeight: 720,
  outputType: "image/webp",
};

/**
 * Reads the provided file and returns a data URL string.
 */
function readFileAsDataURL(file: File): Promise<string> {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = () => resolve(reader.result as string);
    reader.onerror = () => reject(reader.error);
    reader.readAsDataURL(file);
  });
}

/**
 * Calculates the original binary size of a base64 encoded data URL string.
 */
function estimateBytesFromDataURL(dataUrl: string): number {
  const parts = dataUrl.split(",", 2);
  if (parts.length <= BASE64_DATA_INDEX) {
    return 0;
  }
  const base64 = parts[BASE64_DATA_INDEX];
  const paddingMatches = base64.match(/=+$/);
  const padding = paddingMatches ? paddingMatches[0].length : 0;
  return (base64.length * 3) / 4 - padding;
}

async function loadImageElement(dataUrl: string): Promise<HTMLImageElement> {
  return new Promise((resolve, reject) => {
    const image = new Image();
    image.decoding = "async";
    image.onload = () => resolve(image);
    image.onerror = () => reject(new Error("Failed to load image for compression"));
    image.src = dataUrl;
  });
}

async function compressDataURL(
  dataUrl: string,
  options: ImageCompressionOptions,
): Promise<string> {
  const image = await loadImageElement(dataUrl);
  const canvas = document.createElement("canvas");
  const context = canvas.getContext("2d");
  if (!context) {
    throw new Error("Failed to acquire 2D canvas context");
  }

  let currentWidth = image.naturalWidth || image.width;
  let currentHeight = image.naturalHeight || image.height;
  let quality = options.initialQuality;
  let lastResult = dataUrl;
  let iteration = 0;

  const drawAndEncode = () => {
    canvas.width = currentWidth;
    canvas.height = currentHeight;
    context.clearRect(0, 0, currentWidth, currentHeight);
    context.drawImage(image, 0, 0, currentWidth, currentHeight);
    return canvas.toDataURL(options.outputType, quality);
  };

  // Limit the number of attempts to avoid infinite loops.
  while (iteration < 20) {
    iteration += 1;
    const candidate = drawAndEncode();
    lastResult = candidate;
    const bytes = estimateBytesFromDataURL(candidate);
    if (bytes <= options.maxBytes) {
      return candidate;
    }

    if (quality > options.minQuality + 0.05) {
      quality = Math.max(quality - options.qualityStep, options.minQuality);
      continue;
    }

    const nextWidth = Math.floor(currentWidth * options.resizeStep);
    const nextHeight = Math.floor(currentHeight * options.resizeStep);
    if (
      nextWidth < options.minWidth ||
      nextHeight < options.minHeight ||
      nextWidth === currentWidth ||
      nextHeight === currentHeight
    ) {
      return candidate;
    }

    currentWidth = nextWidth;
    currentHeight = nextHeight;
    quality = options.initialQuality;
  }

  return lastResult;
}

/**
 * Reads the provided file. If its size exceeds the configured threshold, the image
 * is compressed before returning the data URL. When compression fails, the original
 * data URL is returned.
 */
export async function prepareImageForStorage(
  file: File,
  customOptions?: Partial<ImageCompressionOptions>,
): Promise<string> {
  const options = { ...DEFAULT_OPTIONS, ...customOptions };
  const originalDataUrl = await readFileAsDataURL(file);

  if (file.size <= options.maxBytes) {
    return originalDataUrl;
  }

  try {
    const compressedDataUrl = await compressDataURL(originalDataUrl, options);
    return compressedDataUrl;
  } catch (error) {
    console.error("[prepareImageForStorage] Failed to compress image:", error);
    return originalDataUrl;
  }
}

