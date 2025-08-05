import { z } from "zod";

const zFloorp11WorkspacesSchema = z.object({
  windows: z.record(
    z.string(),
    z.record(
      z.string(),
      z
        .object({
          name: z.string(),
          tabs: z.array(z.undefined()),
          defaultWorkspace: z.boolean(),
          id: z.string(),
          icon: z.string().nullable(),
        })
        .or(
          z.object({
            selectedWorkspaceId: z.string(),
          }),
        ),
    ),
  ),
});
