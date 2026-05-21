// SPDX-License-Identifier: MPL-2.0

import * as v from 'valibot';
import { commands } from './commands.ts';

// KeyCodes list
export const keyCodesList = {
  F1: ["F1", "VK_F1"],
  F2: ["F2", "VK_F2"],
  F3: ["F3", "VK_F3"],
  F4: ["F4", "VK_F4"],
  F5: ["F5", "VK_F5"],
  F6: ["F6", "VK_F6"],
  F7: ["F7", "VK_F7"],
  F8: ["F8", "VK_F8"],
  F9: ["F9", "VK_F9"],
  F10: ["F10", "VK_F10"],
  F11: ["F11", "VK_F11"],
  F12: ["F12", "VK_F12"],
  F13: ["F13", "VK_F13"],
  F14: ["F14", "VK_F14"],
  F15: ["F15", "VK_F15"],
  F16: ["F16", "VK_F16"],
  F17: ["F17", "VK_F17"],
  F18: ["F18", "VK_F18"],
  F19: ["F19", "VK_F19"],
  F20: ["F20", "VK_F20"],
  F21: ["F21", "VK_F21"],
  F22: ["F22", "VK_F22"],
  F23: ["F23", "VK_F23"],
  F24: ["F24", "VK_F24"],
} as const;

// FloorpCSKData schema
export const FloorpCSKData = v.array(
  v.object({
    actionName: v.string(),
    key: v.string(),
    keyCode: v.string(),
    modifiers: v.string(),
  })
);
export type FloorpCSKData = v.InferOutput<typeof FloorpCSKData>;

const cskEntrySchema = v.object({
  modifiers: v.object({
    alt: v.boolean(),
    ctrl: v.boolean(),
    meta: v.boolean(),
    shift: v.boolean(),
  }),
  key: v.string(),
});

// CSKData schema
export const CSKDataCodec = v.record(
  v.picklist(Object.keys(commands) as [string, ...string[]]),
  cskEntrySchema,
);
export type CSKData = Record<string, v.InferOutput<typeof cskEntrySchema>>;

// CSKCommands schema
export const CSKCommandsCodec = v.union([
  v.object({
    type: v.literal('disable-csk'),
    data: v.boolean(),
  }),
  v.object({
    type: v.literal('update-pref'),
  }),
]);
export type CSKCommands = v.InferOutput<typeof CSKCommandsCodec>;

// Function replacing zCSKData.parse / floorpCSKToNora
export function floorpCSKToNora(data: FloorpCSKData): CSKData {
  const arr: Record<string, { key: string; modifiers: { alt: boolean; ctrl: boolean; meta: boolean; shift: boolean } }> = {};

  for (const datum of data) {
    arr[datum.actionName] = {
      key: datum.keyCode ? datum.keyCode.replace('VK_', '') : datum.key,
      modifiers: {
        alt: datum.modifiers.includes('alt'),
        ctrl: datum.modifiers.includes('ctrl'),
        meta: false,
        shift: datum.modifiers.includes('shift'),
      },
    };
  }

  const result = v.safeParse(CSKDataCodec, arr);
  if (!result.success) {
    throw new Error('Invalid CSKData: ' + JSON.stringify(result.issues));
  }
  return result.output as CSKData;
}

// Parse helpers (consumers don't need to import valibot directly)
export function parseCSKData(input: unknown): CSKData {
  const result = v.safeParse(CSKDataCodec, input);
  if (!result.success) {
    throw new Error('Invalid CSKData: ' + JSON.stringify(result.issues));
  }
  return result.output as CSKData;
}

export function parseCSKCommands(input: unknown): CSKCommands | null {
  const result = v.safeParse(CSKCommandsCodec, input);
  if (!result.success) {
    return null;
  }
  return result.output;
}
