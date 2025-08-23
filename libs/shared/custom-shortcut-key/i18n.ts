import { commands } from "./commands";
import type { CSKData } from "./defines";

export function getFluentLocalization(actionName: keyof CSKData) {
  if (!commands[actionName]) {
    console.error(`actionName(${actionName}) do not exists.`);
    return null;
  }
  //TODO:
  //return `floorp-custom-actions-${commands[actionName][1]}`;
}
