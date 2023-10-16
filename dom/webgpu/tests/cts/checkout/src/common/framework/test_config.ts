export type TestConfig = {
  maxSubcasesInFlight: number;
  testHeartbeatCallback: () => void;
  noRaceWithRejectOnTimeout: boolean;

  /**
   * Controls the emission of loops in constant-evaluation shaders under
   * 'webgpu:shader,execution,expression,*'
   * FXC is extremely slow to compile shaders with loops unrolled, where as the
   * MSL compiler is extremely slow to compile with loops rolled.
   */
  unrollConstEvalLoops: boolean;
};

export const globalTestConfig: TestConfig = {
  maxSubcasesInFlight: 500,
  testHeartbeatCallback: () => {},
  noRaceWithRejectOnTimeout: false,
  unrollConstEvalLoops: false,
};
