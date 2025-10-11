/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { useEffect, useState } from "react";
import { Button } from "@/components/common/button.tsx";
import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import {
  AlertCircle,
  CheckCircle,
  Download,
  Power,
  XCircle,
} from "lucide-react";

declare global {
  var OSAutomotor: {
    isPlatformSupported(): Promise<boolean>;
    isEnabled(): Promise<boolean>;
    getInstalledVersion(): Promise<string>;
    enable(): Promise<{ success: boolean; error?: string }>;
    disable(): Promise<{ success: boolean; error?: string }>;
    getStatus(): Promise<{
      enabled: boolean;
      platformSupported: boolean;
      installedVersion: string;
    }>;
    getPlatformDebugInfo(): Promise<{
      os: string;
      arch: string;
      supported: boolean;
    }>;
  };
}

interface FloorpOSStatus {
  enabled: boolean;
  platformSupported: boolean;
  installedVersion: string;
}

export default function FloorpOS() {
  const [status, setStatus] = useState<FloorpOSStatus | null>(null);
  const [loading, setLoading] = useState(false);
  const [message, setMessage] = useState("");
  const [error, setError] = useState("");

  useEffect(() => {
    loadStatus();
  }, []);

  const loadStatus = async () => {
    try {
      if (!globalThis.OSAutomotor) {
        setError("OSAutomotor API is not available");
        return;
      }

      const newStatus = await globalThis.OSAutomotor.getStatus();
      setStatus(newStatus);
      setError("");
    } catch (err) {
      console.error("Failed to load status:", err);
      setError(String(err));
    }
  };

  const handleEnable = async () => {
    setLoading(true);
    setMessage("Enabling Floorp OS...");
    setError("");

    try {
      const result = await globalThis.OSAutomotor.enable();
      if (result.success) {
        setMessage("Floorp OS enabled successfully!");
        await loadStatus();
      } else {
        setError(result.error || "Failed to enable Floorp OS");
        setMessage("");
      }
    } catch (err) {
      console.error("Error enabling Floorp OS:", err);
      setError(String(err));
      setMessage("");
    } finally {
      setLoading(false);
    }
  };

  const handleDisable = async () => {
    setLoading(true);
    setMessage("Disabling Floorp OS...");
    setError("");

    try {
      const result = await globalThis.OSAutomotor.disable();
      if (result.success) {
        setMessage("Floorp OS disabled successfully!");
        await loadStatus();
      } else {
        setError(result.error || "Failed to disable Floorp OS");
        setMessage("");
      }
    } catch (err) {
      console.error("Error disabling Floorp OS:", err);
      setError(String(err));
      setMessage("");
    } finally {
      setLoading(false);
    }
  };

  if (!status) {
    return (
      <div className="p-6 space-y-3">
        <div className="flex flex-col items-start pl-6">
          <h1 className="text-3xl font-bold mb-2">Floorp OS</h1>
          <p className="text-sm mb-8">
            Loading...
          </p>
        </div>
      </div>
    );
  }

  return (
    <div className="p-6 space-y-3">
      <div className="flex flex-col items-start pl-6">
        <h1 className="text-3xl font-bold mb-2">Floorp OS</h1>
        <p className="text-sm mb-8">
          Manage Floorp OS backend modules and services
        </p>
      </div>

      <div className="space-y-3 pl-6">
        {/* Status Card */}
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              Status
              {status.enabled
                ? (
                  <span className="ml-2 inline-flex items-center rounded-full bg-green-100 px-2.5 py-0.5 text-xs font-medium text-green-800 dark:bg-green-900 dark:text-green-100">
                    <CheckCircle className="mr-1 h-3 w-3" />
                    Enabled
                  </span>
                )
                : (
                  <span className="ml-2 inline-flex items-center rounded-full bg-gray-100 px-2.5 py-0.5 text-xs font-medium text-gray-800 dark:bg-gray-800 dark:text-gray-100">
                    <XCircle className="mr-1 h-3 w-3" />
                    Disabled
                  </span>
                )}
            </CardTitle>
            <CardDescription>
              Current status of Floorp OS
            </CardDescription>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="grid gap-4">
              <div className="flex items-center justify-between">
                <div className="space-y-1">
                  <p className="text-sm font-medium">Platform Support</p>
                  <p className="text-sm text-muted-foreground">
                    {status.platformSupported
                      ? "Your platform is supported"
                      : "Your platform is not supported"}
                  </p>
                </div>
                {status.platformSupported
                  ? <CheckCircle className="h-5 w-5 text-green-500" />
                  : <XCircle className="h-5 w-5 text-red-500" />}
              </div>

              <div className="flex items-center justify-between">
                <div className="space-y-1">
                  <p className="text-sm font-medium">Installed Version</p>
                  <p className="text-sm text-muted-foreground">
                    {status.installedVersion || "Not installed"}
                  </p>
                </div>
                {status.installedVersion && (
                  <Download className="h-5 w-5 text-blue-500" />
                )}
              </div>

              <div className="flex items-center justify-between">
                <div className="space-y-1">
                  <p className="text-sm font-medium">Service Status</p>
                  <p className="text-sm text-muted-foreground">
                    {status.enabled ? "Running" : "Stopped"}
                  </p>
                </div>
                <Power
                  className={`h-5 w-5 ${
                    status.enabled ? "text-green-500" : "text-gray-400"
                  }`}
                />
              </div>
            </div>
          </CardContent>
        </Card>

        {/* Controls Card */}
        <Card>
          <CardHeader>
            <CardTitle>Controls</CardTitle>
            <CardDescription>
              Enable or disable Floorp OS
            </CardDescription>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="flex gap-4">
              <Button
                onClick={handleEnable}
                disabled={loading ||
                  !status.platformSupported ||
                  status.enabled}
                className="flex-1"
              >
                <Power className="mr-2 h-4 w-4" />
                Enable Floorp OS
              </Button>
              <Button
                onClick={handleDisable}
                disabled={loading || !status.enabled}
                variant="secondary"
                className="flex-1"
              >
                <Power className="mr-2 h-4 w-4" />
                Disable Floorp OS
              </Button>
            </div>

            <Button
              onClick={loadStatus}
              disabled={loading}
              variant="secondary"
              className="w-full"
            >
              Refresh Status
            </Button>

            {message && (
              <div className="flex items-center gap-2 p-3 bg-blue-50 dark:bg-blue-950 text-blue-900 dark:text-blue-100 rounded-md">
                <CheckCircle className="h-4 w-4" />
                <p className="text-sm">{message}</p>
              </div>
            )}

            {error && (
              <div className="flex items-center gap-2 p-3 bg-red-50 dark:bg-red-950 text-red-900 dark:text-red-100 rounded-md">
                <AlertCircle className="h-4 w-4" />
                <p className="text-sm">{error}</p>
              </div>
            )}
          </CardContent>
        </Card>
      </div>
    </div>
  );
}
