import * as t from 'io-ts';
import { commands } from './commands';
import { getOrElseW } from 'fp-ts/Either';
import { pipe } from 'fp-ts/function';

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

// FloorpCSKData codec
export const FloorpCSKData = t.array(
  t.type({
    actionName: t.string,
    key: t.string,
    keyCode: t.string,
    modifiers: t.string,
  })
);
export type FloorpCSKData = t.TypeOf<typeof FloorpCSKData>;

// zCSKData equivalent in io-ts
export const CSKDataCodec = t.record(
  t.keyof(Object.keys(commands).reduce((acc, k) => {
    acc[k] = null;
    return acc;
  }, {} as Record<string, null>)),
  t.type({
    modifiers: t.type({
      alt: t.boolean,
      ctrl: t.boolean,
      meta: t.boolean,
      shift: t.boolean,
    }),
    key: t.string,
  })
);
export type CSKData = t.TypeOf<typeof CSKDataCodec>;

// Function replacing zCSKData.parse
export function floorpCSKToNora(data: FloorpCSKData): CSKData {
  const arr: Record<string, { key: string; modifiers: CSKData['any']['modifiers'] }> = {};
  
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

  return pipe(
    CSKDataCodec.decode(arr),
    getOrElseW(errors => {
      throw new Error(`Invalid CSKData: ${JSON.stringify(errors)}`);
    })
  );
}

// zCSKCommands equivalent in io-ts
export const CSKCommandsCodec = t.union([
  t.type({
    type: t.keyof({ 'disable-csk': null }),
    data: t.boolean,
  }),
  t.type({
    type: t.keyof({ 'update-pref': null }),
  }),
]);
export type CSKCommands = t.TypeOf<typeof CSKCommandsCodec>;
