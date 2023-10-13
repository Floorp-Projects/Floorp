export type TestConfig = {
  maxSubcasesInFlight: number;
  testHeartbeatCallback: () => void;
  noRaceWithRejectOnTimeout: boolean;

  /**
   * Logger for debug messages from the test framework
   * (that can't be captured in the logs of a test).
   */
  frameworkDebugLog?: (msg: string) => void;

  /**
   * Controls the emission of loops in constant-evaluation shaders under
   * 'webgpu:shader,execution,expression,*'
   * FXC is extremely slow to compile shaders with loops unrolled, where as the
   * MSL compiler is extremely slow to compile with loops rolled.
   */
  unrollConstEvalLoops: boolean;

  /**
   * Whether or not we're running in compatibility mode.
   */
  compatibility: boolean;
};

export const globalTestConfig: TestConfig = {
  maxSubcasesInFlight: 500,
  testHeartbeatCallback: () => {},
  noRaceWithRejectOnTimeout: false,
  unrollConstEvalLoops: false,
  compatibility: false,
};
