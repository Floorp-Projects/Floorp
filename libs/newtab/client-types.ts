// SPDX-License-Identifier: MPL-2.0
export interface NewTabBridge {
  NRNewTabSend?: (message: string) => void;
  NRNewTabRegisterReceiveCallback?: (
    callback: (message: string) => void,
  ) => void;
}
export interface PendingRequest {
  resolve: (value: unknown) => void;
  reject: (error: Error) => void;
  timer: ReturnType<typeof setTimeout>;
}
