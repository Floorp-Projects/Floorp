import type { TSetting, TSettingsCard } from "../types/settings_format.d.ts";
import {
  Card,
  CardContent,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";

function Input(setting: TSetting) {
  switch (setting.settingsType) {
    case "input":
      return (
        <input type="input" defaultValue={setting.default as string | number} />
      );
    case "radio":
      return <input type="radio" defaultValue={setting.default as string} />;
    case "select":
      return (
        <select defaultValue={setting.default as string}>
          {setting.options!.map((o) => (
            <option key={o} value={o}>
              {o}
            </option>
          ))}
        </select>
      );
    case "checkbox":
      return (
        <input type="checkbox" defaultChecked={setting.default as boolean} />
      );
    default:
      return setting.settingsType satisfies never;
  }
}

export function SettingsCard(props: TSettingsCard) {
  return props.settings.map((setting, index) => (
    <Card key={index}>
      <CardHeader>
        <CardTitle>{props.title}</CardTitle>
      </CardHeader>
      <CardContent>
        {Input(setting)}
        <p>{props.description}</p>
      </CardContent>
    </Card>
  ));
}