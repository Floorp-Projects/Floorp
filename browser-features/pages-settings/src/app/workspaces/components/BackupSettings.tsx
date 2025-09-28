import {
  Card,
  CardContent,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { useTranslation } from "react-i18next";
import { SaveAll } from "lucide-react";

export function BackupSettings() {
  const { t } = useTranslation();

  return (
    <Card>
      <CardHeader>
        <CardTitle className="flex items-center gap-2">
          <SaveAll className="size-5" />
          {t("workspaces.backup")}
        </CardTitle>
      </CardHeader>
      <CardContent className="space-y-3">
        <div>
          <h3 className="text-base font-medium">
            {t("workspaces.backupComingSoon")}
          </h3>
          <p className="text-sm text-base-content/70 mt-1">
            {t("workspaces.backupDescription")}
          </p>
        </div>

        <div className="p-3 bg-base-200 rounded-lg">
          <p className="text-sm">
            {t("workspaces.backupComingSoon")}
          </p>
        </div>
      </CardContent>
    </Card>
  );
}
