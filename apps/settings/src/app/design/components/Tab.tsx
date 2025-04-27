import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card.tsx";
import { Input } from "@/components/ui/input.tsx";
import { useTranslation } from "react-i18next";
import { useFormContext } from "react-hook-form";

export function Tab() {
    const { t } = useTranslation();
    const { getValues, setValue } = useFormContext();

    return (
        <Card>
            <CardHeader>
                <CardTitle>{t("design.tab.title")}</CardTitle>
            </CardHeader>
            <CardContent className="space-y-6">
                <div>
                    <h3 className="text-base font-medium mb-2">{t("design.tab.scroll")}</h3>
                    <p className="text-sm text-muted-foreground mb-4">{t("design.tab.scrollDescription")}</p>
                    <div className="space-y-4">
                        <div className="flex items-center justify-between">
                            <label>{t("design.tab.scrollTab")}</label>
                            <input
                                type="checkbox"
                                checked={!!getValues("tabScroll")}
                                onChange={(e) => setValue("tabScroll", e.target.checked)}
                            />
                        </div>
                        <div className="flex items-center justify-between">
                            <label>{t("design.tab.reverseScroll")}</label>
                            <input
                                type="checkbox"
                                disabled={!getValues("tabScroll")}
                                checked={!!getValues("tabScrollReverse")}
                                onChange={(e) => setValue("tabScrollReverse", e.target.checked)}
                            />
                        </div>
                        <div className="flex items-center justify-between">
                            <label>{t("design.tab.scrollWrap")}</label>
                            <input
                                type="checkbox"
                                disabled={!getValues("tabScroll")}
                                checked={!!getValues("tabScrollWrap")}
                                onChange={(e) => setValue("tabScrollWrap", e.target.checked)}
                            />
                        </div>
                    </div>
                </div>

                {!getValues("tabScroll") && (getValues("tabScrollReverse") || getValues("tabScrollWrap")) && (
                    <div className="p-3 bg-muted rounded-lg">
                        <p className="text-sm">
                            {t("design.tab.scrollPrefInfo", {
                                reverseScrollPrefName: t("design.tab.reverseScroll"),
                                scrollWrapPrefName: t("design.tab.scrollWrap"),
                                scrollPrefName: t("design.tab.scrollTab"),
                            })}
                        </p>
                    </div>
                )}

                <div>
                    <h3 className="text-base font-medium mb-2">{t("design.tab.openPosition")}</h3>
                    <p className="text-sm text-muted-foreground mb-4">{t("design.tab.openPositionDescription")}</p>
                    <div className="flex flex-col space-y-2">
                        {[
                            { value: -1, label: "design.tab.openDefault" },
                            { value: 0, label: "design.tab.openLast" },
                            { value: 1, label: "design.tab.openNext" }
                        ].map(({ value, label }) => (
                            <label key={value} className="flex items-center space-x-2">
                                <input
                                    type="radio"
                                    name="tabOpenPosition"
                                    value={value}
                                    checked={getValues("tabOpenPosition") === value}
                                    onChange={(e) => setValue("tabOpenPosition", Number(e.target.value))}
                                />
                                <span>{t(label)}</span>
                            </label>
                        ))}
                    </div>
                </div>

                <div className="flex items-center justify-between">
                    <label>{t("design.tab.pinTitle")}</label>
                    <input
                        type="checkbox"
                        checked={!!getValues("tabPinTitle")}
                        onChange={(e) => setValue("tabPinTitle", e.target.checked)}
                    />
                </div>

                <div className="flex items-center justify-between">
                    <label>{t("design.tab.doubleClickToClose")}</label>
                    <input
                        type="checkbox"
                        checked={!!getValues("tabDubleClickToClose")}
                        onChange={(e) => setValue("tabDubleClickToClose", e.target.checked)}
                    />
                </div>

                <div>
                    <div className="flex justify-between items-center mb-2">
                        <label>{t("design.tab.minWidth")}</label>
                        <span className="text-sm text-muted-foreground">60px - 300px</span>
                    </div>
                    <Input
                        type="number"
                        value={getValues("tabMinWidth") || ""}
                        onChange={(e: React.ChangeEvent<HTMLInputElement>) => setValue("tabMinWidth", Number(e.target.value))}
                        className="w-full"
                    />
                    {(getValues("tabMinWidth") < 60 || getValues("tabMinWidth") > 300) && (
                        <p className="text-destructive text-sm mt-1">
                            {t("design.tab.minWidthError", { min: 60, max: 300 })}
                        </p>
                    )}
                </div>

                <div>
                    <div className="flex justify-between items-center mb-2">
                        <label>{t("design.tab.minHeight")}</label>
                        <span className="text-sm text-muted-foreground">20px - 100px</span>
                    </div>
                    <Input
                        type="number"
                        value={getValues("tabMinHeight") || ""}
                        onChange={(e: React.ChangeEvent<HTMLInputElement>) => setValue("tabMinHeight", Number(e.target.value))}
                        className="w-full"
                    />
                    {(getValues("tabMinHeight") < 20 || getValues("tabMinHeight") > 100) && (
                        <p className="text-destructive text-sm mt-1">
                            {t("design.tab.minHeightError", { min: 20, max: 100 })}
                        </p>
                    )}
                </div>
            </CardContent>
        </Card>
    );
}