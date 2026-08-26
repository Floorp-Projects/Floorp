const MAX_WIDTH = 480;
const MAX_HEIGHT = 960;
const INITIAL_QUALITY = 0.7;
const MIN_QUALITY = 0.3;
const MAX_DATA_URL_BYTES = 200 * 1024; // 200KB

export function compressImage(source: File | Blob): Promise<string> {
    return new Promise((resolve, reject) => {
        const img = new Image();
        const url = URL.createObjectURL(source);

        img.onload = () => {
            URL.revokeObjectURL(url);

            let { width, height } = img;
            if (width > MAX_WIDTH) {
                height = Math.round((height * MAX_WIDTH) / width);
                width = MAX_WIDTH;
            }
            if (height > MAX_HEIGHT) {
                width = Math.round((width * MAX_HEIGHT) / height);
                height = MAX_HEIGHT;
            }

            const canvas = document.createElement("canvas");
            canvas.width = width;
            canvas.height = height;

            const ctx = canvas.getContext("2d");
            if (!ctx) {
                reject(new Error("Canvas 2D context unavailable"));
                return;
            }

            ctx.drawImage(img, 0, 0, width, height);

            // Re-compress at lower quality if result exceeds size limit
            let quality = INITIAL_QUALITY;
            let dataUrl = canvas.toDataURL("image/jpeg", quality);

            while (dataUrl.length > MAX_DATA_URL_BYTES && quality > MIN_QUALITY) {
                quality -= 0.1;
                dataUrl = canvas.toDataURL("image/jpeg", quality);
            }

            if (dataUrl.length > MAX_DATA_URL_BYTES) {
                reject(new Error("Image too large even after compression"));
                return;
            }

            resolve(dataUrl);
        };

        img.onerror = () => {
            URL.revokeObjectURL(url);
            reject(new Error("Failed to load image"));
        };

        img.src = url;
    });
}
