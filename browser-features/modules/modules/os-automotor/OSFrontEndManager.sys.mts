/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * OSFrontEndManager - Manages the workflow progress window
 *
 * Progress state is stored here and queried by the progress window via Actor.
 */

const { BrowserWindowTracker } = ChromeUtils.importESModule(
  "resource:///modules/BrowserWindowTracker.sys.mjs",
);

const { setTimeout, setInterval, clearInterval } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);

const PROGRESS_WINDOW_URL = "http://localhost:5192/";

interface WorkflowProgressStep {
  id: string;
  functionId: string;
  functionName: string;
  status: "pending" | "running" | "completed" | "error";
  startedAt?: number;
  completedAt?: number;
}

interface WorkflowProgressMessage {
  type:
    | "workflow-progress-start"
    | "workflow-progress-update"
    | "workflow-progress-complete"
    | "workflow-progress-error";
  workflowId: string;
  workflowName?: string;
  steps?: WorkflowProgressStep[];
  currentStepIndex?: number;
  status?: "idle" | "running" | "completed" | "error";
}

interface WorkflowProgressState {
  workflowId: string;
  workflowName: string;
  steps: WorkflowProgressStep[];
  currentStepIndex: number;
  status: "idle" | "running" | "completed" | "error";
}

class OSFrontEndManager {
  private static _instance: OSFrontEndManager | null = null;
  private _progressWindow: Window | null = null;
  private _currentProgress: WorkflowProgressState | null = null;

  static getInstance(): OSFrontEndManager {
    if (!OSFrontEndManager._instance) {
      OSFrontEndManager._instance = new OSFrontEndManager();
    }
    return OSFrontEndManager._instance;
  }

  /**
   * Get current progress state (called by Actor)
   */
  public getCurrentProgress(): WorkflowProgressState | null {
    return this._currentProgress;
  }

  /**
   * Handle workflow progress message from Actor
   */
  public handleProgressMessage(data: WorkflowProgressMessage): void {
    this._currentProgress = {
      workflowId: data.workflowId || "",
      workflowName: data.workflowName || "Workflow",
      steps: data.steps || [],
      currentStepIndex: data.currentStepIndex ?? -1,
      status: data.status || "idle",
    };

    if (data.type === "workflow-progress-start") {
      this.openProgressWindow();
    }
  }

  /**
   * Open the progress window (excluded from session restore like PWA windows)
   */
  public async openProgressWindow(): Promise<void> {
    if (this._progressWindow && !this._progressWindow.closed) {
      this._progressWindow.focus();
      return;
    }

    try {
      // Get screen dimensions for positioning
      const recentWindow = Services.wm.getMostRecentWindow("navigator:browser") as Window | null;
      const screenWidth = recentWindow?.screen?.availWidth || 1920;
      const width = 360;
      const height = 480;
      const left = screenWidth - (width + 20); // 20px margin from right

      // Create extraOptions to exclude from session restore (like PWA/TaskBarTabs)
      const extraOptions = Cc["@mozilla.org/hash-property-bag;1"].createInstance(
        Ci.nsIWritablePropertyBag2,
      );
      extraOptions.setPropertyAsAString("taskbartab", "workflow-progress");

      // Create URL argument
      const url = Cc["@mozilla.org/supports-string;1"].createInstance(
        Ci.nsISupportsString,
      );
      url.data = PROGRESS_WINDOW_URL;

      // Build args array
      const args = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
      args.appendElement(url);
      args.appendElement(extraOptions);

      // Window features based on PictureInPicture implementation
      const windowFeatures = [
        "chrome",
        "titlebar=no",
        "close",
        "dialog", // Try adding dialog feature
        "toolbar=no",
        "location=no",
        "personalbar=no",
        "menubar=no",
        "resizable",
        "minimizable",
        "alwaysRaised",
        "alwaysontop",
        `outerWidth=${width}`,
        `outerHeight=${height}`,
        `left=${left}`,
        `top=80`,
      ].join(",");
      
      console.log("[OSFrontEndManager] Opening window via BrowserWindowTracker...");
      
      const win = await BrowserWindowTracker.promiseOpenWindow({
        args,
        features: windowFeatures,
        all: false,
      });

      this._progressWindow = win;

      // Debug: Log screen info
      const targetScreen = win.screen;
      // Recalculate position based on the actual window's screen
      const realWidth = 360;
      const realHeight = targetScreen.availHeight;
      
      // Note: win.screenX/Y in main process might be different from content process, but win.screen gives the screen object.
      // availLeft/Top are crucial for multi-monitor setups.
      console.log(`[OSFrontEndManager] Screen info: availLeft=${targetScreen.availLeft}, availTop=${targetScreen.availTop}, availWidth=${targetScreen.availWidth}, availHeight=${targetScreen.availHeight}`);
      
      // Place at top-right corner without margin
      const targetLeft = targetScreen.availLeft + targetScreen.availWidth - realWidth; 
      const targetTop = targetScreen.availTop;

      // Force position logic
      const forcePosition = () => {
         try {
            win.moveTo(targetLeft, targetTop);
            if (win.outerWidth !== realWidth || win.outerHeight !== realHeight) {
                win.resizeTo(realWidth, realHeight);
            }
         } catch(e) {
            console.error("[OSFrontEndManager] Position error:", e);
         }
      };

      // Force position repeatedly
      const intervalId = setInterval(forcePosition, 100);
      setTimeout(() => clearInterval(intervalId), 1000);

    } catch (error) {
      console.error("[OSFrontEndManager] Failed to open window:", error);
    }
  }


  /**
   * Close the progress window
   */
  public closeProgressWindow(): void {
    if (this._progressWindow && !this._progressWindow.closed) {
      this._progressWindow.close();
    }
    this._progressWindow = null;
    this._currentProgress = null;
  }
}

export const osFrontEndManager = OSFrontEndManager.getInstance();
