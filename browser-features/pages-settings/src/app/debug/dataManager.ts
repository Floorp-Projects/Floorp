// OneDriveのファイル情報を取得
export async function getOneDriveFileNameList(): Promise<string[]> {
  console.log("dataManager: getOneDriveFileNameList called");

  return await new Promise((resolve) => {
    const timeoutId = setTimeout(() => {
      console.error("dataManager: Timeout waiting for OneDrive file list");
      resolve([]);
    }, 30000);

    if (window.NRGetOneDriveFileNameList) {
      console.log(
        "dataManager: NRGetOneDriveFileNameList is available, calling it",
      );

      window.NRGetOneDriveFileNameList((data: string) => {
        clearTimeout(timeoutId);

        console.log("dataManager: Received data from OneDrive callback");
        try {
          const oneDriveFileNameList = JSON.parse(data) as string[];
          console.log(
            `dataManager: Parsed ${oneDriveFileNameList.length} files`,
          );
          console.log(
            "dataManager: First few files:",
            oneDriveFileNameList.slice(0, 5),
          );
          resolve(oneDriveFileNameList);
        } catch (error) {
          console.error("dataManager: Error parsing OneDrive data:", error);
          console.error("dataManager: Data received:", data);
          resolve([]);
        }
      });
    } else {
      clearTimeout(timeoutId);

      console.error("dataManager: NRGetOneDriveFileNameList is not available");
      const dummyData = [
        "ダミーファイル1.docx",
        "ダミーファイル2.xlsx",
        "ダミーファイル3.pdf",
      ];
      resolve(dummyData);
    }
  });
}

// Gmail送信を開始
export async function sendGmailViaActor(
  to: string,
  subject: string,
  body: string,
): Promise<void> {
  console.log("dataManager: sendGmailViaActor called with:", {
    to,
    subject,
    body,
  });

  return await new Promise((resolve, reject) => {
    // タイムアウトを設定 (例: 60秒)
    const timeoutId = setTimeout(() => {
      console.error("dataManager: Timeout waiting for Gmail send result");
      reject(new Error("Timeout waiting for Gmail send result"));
    }, 60000);

    if (window.NRSendGmail) {
      console.log("dataManager: NRSendGmail is available, calling it");
      window.NRSendGmail(to, subject, body, (result) => {
        clearTimeout(timeoutId);
        console.log(
          "dataManager: Received result from Gmail callback:",
          result,
        );
        if (result.success) {
          resolve();
        } else {
          reject(new Error(result.message || "Gmail sending failed"));
        }
      });
    } else {
      clearTimeout(timeoutId);
      console.error("dataManager: NRSendGmail is not available");
      // 開発用のダミー処理やエラーハンドリング
      reject(new Error("NRSendGmail function is not available"));
    }
  });
}

declare global {
  interface Window {
    NRGetOneDriveFileNameList?: (callback: (data: string) => void) => void;
    NRSendGmail?: (
      to: string,
      subject: string,
      body: string,
      callback: (result: { success: boolean; message?: string }) => void,
    ) => void;
  }
}
